// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ThirdPersonExCameraMode.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Data/Camera/CameraModeParam/ThirdPersonExCameraModeParamRow.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

namespace
{
	const FName ExCameraSweepIgnoreTag(TEXT("CameraSweepIgnore"));
}

void UThirdPersonExCameraMode::InitializeFromRow( const FThirdPersonExCameraModeParamRow& Row )
{
	CommonParams = Row;

	Priority = Row.Priority;	// 基底のアクティブ選択で使う

	TargetArmLength = Row.TargetArmLength;
	TargetOffset = Row.TargetOffset;
	FocusHeightOffset = Row.FocusHeightOffset;
	CollisionRadius = Row.CollisionRadius;
	CollisionPullInInterpSpeed = Row.CollisionPullInInterpSpeed;
	CollisionPullOutInterpSpeed = Row.CollisionPullOutInterpSpeed;

	bEnableCameraLagXY = Row.bEnableCameraLagXY;
	CameraLagSpeedXY = Row.CameraLagSpeedXY;
	bEnableCameraLagZ = Row.bEnableCameraLagZ;
	CameraLagSpeedZ = Row.CameraLagSpeedZ;
	CameraLagMaxDistance = Row.CameraLagMaxDistance;
	bEnableCameraRotationLag = Row.bEnableCameraRotationLag;
	CameraRotationLagSpeed = Row.CameraRotationLagSpeed;

	// ジャンプ注視点（縦デッドゾーン）は TidePlayerParamDataAsset へ集約済みのため行からコピーしない
}

void UThirdPersonExCameraMode::UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo )
{
	FRotator BaseRotation = ControlData.ControlRotation;
	FVector BaseFocusLocation = ControlData.TargetLocation;

	ACharacter* PlayerChar = Cast<ACharacter>( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) );
	if ( !PlayerChar ) return;

	ATidePlayerCharacter* TidePlayer = Cast<ATidePlayerCharacter>( PlayerChar );
	if ( !TidePlayer ) return;

	// リスタート等で要求される「カメラ正面リセット」は [R3] で一時マスク中（常に false）。復帰するときは
	// TidePlayer->ConsumeCameraResetRequest() の戻り値を入れる（暗転中に呼ばれる前提なので補間せず即スナップ）
	const bool bResetToFront = false;

	// デバッグフラグ時は FOV／アーム長を "Default" モードの数値へ固定する
	float FinalFOV = CommonParams.FOV;
	float FinalArmLength = TargetArmLength;
	float FinalPullBackDistance = CurrentPullBackDistance;

	if ( UTideGameSettings::Get()->bDebugFlagDisableFOVChange )
	{
		FinalFOV = 50.0f;
		FinalArmLength = 500.0f;

		FinalPullBackDistance = 0.0f;	// 距離に応じた自動ズームアウトも切る
	}

	// CustomTimeDilation と GlobalTimeDilation を加味した DeltaTime
	const float PlayerDeltaTime = PlayerChar->GetActorTimeDilation() * ControlData.DeltaTime;

	// 面沿いモードは自己完結のビューを組むため、成立したらこの段で完結させる
	if ( TryUpdateSurfaceRideChaseCamera( ControlData, PlayerChar, TidePlayer, PlayerDeltaTime, FinalFOV, OutViewInfo ) )
	{
		return;
	}

	const bool bIsLockOnFollowActive = UpdateLockOnFraming( ControlData, PlayerChar, TidePlayer, PlayerDeltaTime, BaseRotation, BaseFocusLocation );

	UpdateJumpFocusVertical( ControlData, PlayerChar, TidePlayer, PlayerDeltaTime, bIsLockOnFollowActive, BaseFocusLocation );

	// カメラ正面リセット：追従・ラグの蓄積を断ち切り、注視点をプレイヤー原点・回転を正面へ即座に合わせる
	// （ロックオン中でも戻す。目的はフェードイン後のカメラスイング防止）
	if ( bResetToFront )
	{
		LastLockedTargetComp = nullptr;
		CurrentTrackingTargetLoc = FVector::ZeroVector;
		CurrentFocusOffset = FVector::ZeroVector;
		CurrentPullBackDistance = 0.0f;
		FinalPullBackDistance = 0.0f;

		// 注視点はプレイヤー原点。縦の置いていき状態もプレイヤー高さで取り直す
		BaseFocusLocation = ControlData.TargetLocation;
		CurrentFocusZ = ControlData.TargetLocation.Z;
		GroundedFocusZ = ControlData.TargetLocation.Z;
		bFocusZInitialized = true;
		bExceededDeadZone = false;
		bPrevAirborne = false;

		// 回転はプレイヤー正面（水平）。コントローラへも同期
		FRotator FrontRotation = PlayerChar->GetActorRotation();
		FrontRotation.Pitch = 0.0f;
		FrontRotation.Roll = 0.0f;
		BaseRotation = FrontRotation;

		if ( APlayerController* PC = Cast<APlayerController>( PlayerChar->GetController() ) )
		{
			PC->SetControlRotation( FrontRotation );
		}
	}

	FRotator SmoothedRotation = BaseRotation;
	FVector SmoothedFocusLocation = BaseFocusLocation;
	ApplyCameraLagAndClamp( PlayerDeltaTime, BaseRotation, BaseFocusLocation, bResetToFront, SmoothedRotation, SmoothedFocusLocation );

	// 向き計算はスウィープ前の理想位置で行う——実位置だと遮蔽で距離が縮んだ瞬間に
	// atan(オフセット/距離) が跳ねてピッチがガクッと動く
	const FVector IdealCameraLocation = SmoothedFocusLocation - ( SmoothedRotation.Vector() * ( FinalArmLength + FinalPullBackDistance ) );

	// 敵との間にある障害物でスウィープが暴発しないよう、開始地点は注視点ではなくプレイヤー位置にする
	const FVector DesiredLocation = ResolveCameraCollision( ControlData.TargetLocation, IdealCameraLocation, PlayerChar, PlayerDeltaTime, bResetToFront );

	OutViewInfo.Location = DesiredLocation;

	// 【回転軸と注視点の分離】カメラ位置は回転軸（SmoothedFocusLocation）基準で決め、注視点はそこから
	// FocusHeightOffset だけずらす。0 以外なら周回は回転軸のまま Pitch だけずれて注視点が画面中心に来る。
	// 適用率は補間する——即時トグルだと切替の瞬間に傾きぶんのピッチが 1 フレームでスナップするため
	if ( bResetToFront )
	{
		CurrentFocusTiltAlpha = 0.0f;
	}
	else
	{
		const float TiltTarget = bIsLockOnFollowActive ? 0.0f : 1.0f;
		CurrentFocusTiltAlpha = FMath::FInterpTo( CurrentFocusTiltAlpha, TiltTarget, PlayerDeltaTime, FocusTiltInterpSpeed );
	}

	FRotator FinalViewRotation = SmoothedRotation;
	const float LookAtDeltaZ = FocusHeightOffset * CurrentFocusTiltAlpha;	// 注視点は回転軸からの相対高さ×適用率
	if ( !FMath::IsNearlyZero( LookAtDeltaZ ) )
	{
		const FVector LookAtLocation = SmoothedFocusLocation + FVector( 0.0f, 0.0f, LookAtDeltaZ );
		FinalViewRotation = ( LookAtLocation - IdealCameraLocation ).Rotation();
	}

	ApplySurfaceRideRoll( TidePlayer, PlayerDeltaTime, FinalViewRotation );

	OutViewInfo.Rotation = FinalViewRotation;

	OutViewInfo.FOV = FinalFOV;

#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings::Get()->bDebugFlagDrawCameraPivotFocus && GetWorld() )
	{
		DrawPivotFocusDebug( ControlData, SmoothedFocusLocation, DesiredLocation, LookAtDeltaZ );
	}
#endif
}

// 通常 TPS の作り（ワールド基準の Yaw/Pitch＋ピッチクランプ）は 1周する筒に合わないため、自己完結の
// 「進行方向背後＋上＝プレイヤーの上」ビューを組んで打ち切る（移動入力もこのカメラ向き基準になる）
bool UThirdPersonExCameraMode::TryUpdateSurfaceRideChaseCamera( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
	float PlayerDeltaTime, float FinalFOV, FMinimalViewInfo& OutViewInfo )
{
	if ( !TidePlayer->IsSurfaceRideChaseCameraActive() || !GetWorld() )
	{
		bChaseRotationInitialized = false;
		return false;
	}

	const FVector Up = TidePlayer->GetSurfaceRideUp().GetSafeNormal();

	// 接平面へ投影した速度から。低速時は機体前方へフォールバック
	FVector Travel = FVector::VectorPlaneProject( PlayerChar->GetVelocity(), Up ).GetSafeNormal();
	if ( Travel.IsNearlyZero() )
	{
		Travel = FVector::VectorPlaneProject( PlayerChar->GetActorForwardVector(), Up ).GetSafeNormal();
	}

	// 前方すら取れないときは通常 TPS へ委ねる（初期化フラグは触らずライド継続とみなす）
	if ( Travel.IsNearlyZero() ) return false;

	// 進行方向を見て上をプレイヤーの上へ。これで視線・ロール（バンク）が一体で決まる
	const FQuat TargetQuat = FRotationMatrix::MakeFromXZ( Travel, Up ).ToQuat();
	if ( !bChaseRotationInitialized )
	{
		CurrentChaseRotation = TargetQuat;
		bChaseRotationInitialized = true;
	}
	CurrentChaseRotation = FMath::QInterpTo( CurrentChaseRotation, TargetQuat, PlayerDeltaTime, SurfaceRideChaseInterpSpeed );

	const FRotator ViewRot = CurrentChaseRotation.Rotator();
	const FVector Focus = ControlData.TargetLocation + Up * SurfaceRideChaseHeightOffset;
	FVector DesiredLoc = Focus - ViewRot.Vector() * SurfaceRideChaseArmLength;

	// 壁へのめり込み回避（注視点起点の単純スイープ）
	if ( CollisionRadius > 0.0f )
	{
		FHitResult Hit;
		FCollisionQueryParams QP( SCENE_QUERY_STAT( CameraSweep ), false );
		QP.AddIgnoredActor( PlayerChar );
		if ( GetWorld()->SweepSingleByChannel( Hit, ControlData.TargetLocation, DesiredLoc,
			FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere( CollisionRadius ), QP ) )
		{
			DesiredLoc = Hit.Location;
		}
	}

	OutViewInfo.Location = DesiredLoc;
	OutViewInfo.Rotation = ViewRot;
	OutViewInfo.FOV = FinalFOV;

	// 移動入力基準（GetInputBasis）がこのカメラ向きと一致するよう ControlRotation を同期する。視覚ロールは
	// OutViewInfo 側で付与済みなのでここでは 0 で渡す（残すと面沿い離脱後の通常カメラが傾いたままになる）
	if ( APlayerController* PC = Cast<APlayerController>( PlayerChar->GetController() ) )
	{
		FRotator ControlSync = ViewRot;
		ControlSync.Roll = 0.0f;
		PC->SetControlRotation( ControlSync );
	}

	// ラグ・壁避け寄せ量の種は、次に通常 TPS へ戻る初回フレームで理想値から取り直す
	bPendingLagSeedFromIdeal = true;
	bCollisionPullInInitialized = false;
	return true;
}

bool UThirdPersonExCameraMode::UpdateLockOnFraming( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
	float PlayerDeltaTime, FRotator& InOutBaseRotation, FVector& InOutBaseFocusLocation )
{
	// 追従中はカメラが意図的に縦の構図を作るため、呼び出し側でジャンプ注視点固定を適用させない
	bool bIsLockOnFollowActive = false;

	ULockOnComponent* LockOnComp = PlayerChar->FindComponentByClass<ULockOnComponent>();
	if ( !LockOnComp ) return bIsLockOnFollowActive;

	ULockOnTargetComponent* TargetComp = LockOnComp->GetTarget();
	if ( !TargetComp )
	{
		// 対象なし：追従の蓄積をほどいて通常カメラへ戻す
		LastLockedTargetComp = nullptr;
		CurrentTrackingTargetLoc = FVector::ZeroVector;

		CurrentFocusOffset = FMath::VInterpTo( CurrentFocusOffset, FVector::ZeroVector, PlayerDeltaTime, FocusInterpSpeed );
		InOutBaseFocusLocation = ControlData.TargetLocation + CurrentFocusOffset;

		CurrentPullBackDistance = FMath::FInterpTo( CurrentPullBackDistance, 0.0f, PlayerDeltaTime, PullBackInterpSpeed );

		UpdateChargeCameraCentering( ControlData, TidePlayer, PlayerDeltaTime, InOutBaseRotation );
		return bIsLockOnFollowActive;
	}

	const bool bIsPlayerMovingCamera = ( TidePlayer && TidePlayer->IsCameraInputActive() );

	const FVector PlayerLoc = ControlData.TargetLocation;
	const FVector ActualTargetLoc = TargetComp->GetTargetLocation() + FVector( 0.0f, 0.0f, TargetZOffset );

	// 手動操作中もキャッシュ更新は裏で継続する（対象が切り替わった瞬間のスナップ防止）
	if ( LastLockedTargetComp != TargetComp )
	{
		const FVector DirToTarget2D = ( ActualTargetLoc - PlayerLoc ).GetSafeNormal2D();
		const FVector CamRight = FRotationMatrix( ControlData.ControlRotation ).GetUnitAxis( EAxis::Y ).GetSafeNormal2D();
		const float InitialSideDot = FVector::DotProduct( DirToTarget2D, CamRight );
		CurrentLockOnSide = ( InitialSideDot >= 0.0f ) ? -1.0f : 1.0f;

		if ( !LastLockedTargetComp.IsValid() )
		{
			CurrentTrackingTargetLoc = ActualTargetLoc;
		}
		LastLockedTargetComp = TargetComp;
	}
	CurrentTrackingTargetLoc = FMath::VInterpTo( CurrentTrackingTargetLoc, ActualTargetLoc, PlayerDeltaTime, TargetTrackingSpeed );

	float TargetSide = ( CurrentLockOnSide >= 0.0f ) ? 1.0f : -1.0f;

	if ( bIsPlayerMovingCamera )
	{
		// 手動操作中：カメラとターゲットの位置関係から左右をリアルタイムに再決定
		const FVector CamRight2D = FRotationMatrix( ControlData.ControlRotation ).GetUnitAxis( EAxis::Y ).GetSafeNormal2D();
		const FVector DirToTarget2D = ( CurrentTrackingTargetLoc - PlayerLoc ).GetSafeNormal2D();

		const float TargetSideDot = FVector::DotProduct( DirToTarget2D, CamRight2D );

		constexpr float CenterThreshold = 0.05f;
		if ( TargetSideDot > CenterThreshold )
		{
			TargetSide = -1.0f;
		}
		else if ( TargetSideDot < -CenterThreshold )
		{
			TargetSide = 1.0f;
		}
	}
	else
	{
		// 通常時：移動入力（スティック）で回り込み方向を決める
		const FVector PlayerInputVector = PlayerChar->GetLastMovementInputVector();
		const FVector LocalInput = ControlData.ControlRotation.UnrotateVector( PlayerInputVector );
		const float PureRightInput = LocalInput.Y;

		if ( PureRightInput > InputThreshold ) TargetSide = 1.0f;
		else if ( PureRightInput < -InputThreshold ) TargetSide = -1.0f;
	}

	// 手動操作中もこの補間が毎フレーム走るよう分岐の外側へ置く
	CurrentLockOnSide = FMath::FInterpTo( CurrentLockOnSide, TargetSide, PlayerDeltaTime, OffsetInterpSpeed );

	if ( bIsPlayerMovingCamera )
	{
		// 手動操作中は完全な通常カメラの挙動：回転は入力そのまま、注視点と引き距離は通常時の値へ滑らかに戻す
		InOutBaseRotation = ControlData.ControlRotation;

		CurrentFocusOffset = FMath::VInterpTo( CurrentFocusOffset, FVector::ZeroVector, PlayerDeltaTime, FocusInterpSpeed );
		InOutBaseFocusLocation = PlayerLoc + CurrentFocusOffset;

		CurrentPullBackDistance = FMath::FInterpTo( CurrentPullBackDistance, 0.0f, PlayerDeltaTime, PullBackInterpSpeed );

		return bIsLockOnFollowActive;
	}

	// 操作停止中：本来のロックオンカメラ。注視点を敵との間に置き縦構図も作るため、ジャンプ注視点固定は無効化する
	bIsLockOnFollowActive = true;

	const FVector TargetLoc = CurrentTrackingTargetLoc;
	const float HeightDiff = FMath::Max( 0.0f, TargetLoc.Z - PlayerLoc.Z );

	FVector TargetFocusLocation = FMath::Lerp( PlayerLoc, TargetLoc, FocusRatio );
	TargetFocusLocation.Z += HeightDiff * GiantEnemyFocusZRatio;

	FVector TargetFocusOffset = TargetFocusLocation - PlayerLoc;
	TargetFocusOffset = TargetFocusOffset.GetClampedToMaxSize( MaxFocusOffsetDistance );

	CurrentFocusOffset = FMath::VInterpTo( CurrentFocusOffset, TargetFocusOffset, PlayerDeltaTime, FocusInterpSpeed );
	InOutBaseFocusLocation = PlayerLoc + CurrentFocusOffset;

	// 距離に応じた動的アングル
	const float DistanceToTarget = FVector::Distance( PlayerLoc, TargetLoc );
	float DynamicYawOffset = FMath::GetMappedRangeValueClamped( FVector2D( RangeMinDistance, RangeMaxDistance ), FVector2D( CloseYawOffset, FarYawOffset ), DistanceToTarget );
	const float DynamicPitchOffset = FMath::GetMappedRangeValueClamped( FVector2D( RangeMinDistance, RangeMaxDistance ), FVector2D( ClosePitchOffset, FarPitchOffset ), DistanceToTarget );

	// 肉薄時は左右回り込み角を 0 へフェード。距離が詰まるほど CloseYawOffset が最大で乗り、
	// 対象通過時の注視方向反転と重なって当たり際に大きくぶれるのを防ぐ
	if ( CloseFramingFadeDistance > 0.0f )
	{
		DynamicYawOffset *= FMath::Clamp( DistanceToTarget / CloseFramingFadeDistance, 0.0f, 1.0f );
	}

	// 対象向きを作るのは水平距離が十分あるときだけ。真上／直近では (TargetLoc-PlayerLoc) の水平成分が
	// 反転・不安定になり 180 度スイングの原因になる
	const FVector ToTarget2D( TargetLoc.X - PlayerLoc.X, TargetLoc.Y - PlayerLoc.Y, 0.0f );
	FRotator LookAtRot;
	if ( ToTarget2D.SizeSquared() > FMath::Square( FMath::Max( LookAtYawHoldDistance, 1.0f ) ) )
	{
		LookAtRot = ToTarget2D.GetSafeNormal().Rotation();
		float RawLookAtPitch = UKismetMathLibrary::FindLookAtRotation( PlayerLoc, TargetLoc ).Pitch;
		RawLookAtPitch = FMath::Min( RawLookAtPitch, MaxLookUpPitch );

		LookAtRot.Pitch = RawLookAtPitch + DynamicPitchOffset;
		LookAtRot.Yaw += ( DynamicYawOffset * CurrentLockOnSide );
	}
	else
	{
		LookAtRot = FRotator( ControlData.ControlRotation.Pitch, ControlData.ControlRotation.Yaw, 0.0f );
	}

	InOutBaseRotation = FMath::RInterpTo( ControlData.ControlRotation, LookAtRot, PlayerDeltaTime, LockOnTrackingSpeed );

	if ( APlayerController* PC = Cast<APlayerController>( PlayerChar->GetController() ) )
	{
		PC->SetControlRotation( InOutBaseRotation );
	}

	const float TargetPullBackDistance = TargetFocusOffset.Size() + ( HeightDiff * GiantEnemyPullBackRatio );
	CurrentPullBackDistance = FMath::FInterpTo( CurrentPullBackDistance, TargetPullBackDistance, PlayerDeltaTime, PullBackInterpSpeed );

	return bIsLockOnFollowActive;
}

void UThirdPersonExCameraMode::UpdateChargeCameraCentering( const FCameraControlData& ControlData, ATidePlayerCharacter* TidePlayer,
	float PlayerDeltaTime, FRotator& InOutBaseRotation )
{
	const bool bCanAutoCenter = TidePlayer->PlayerParamData &&
		TidePlayer->PlayerParamData->bEnableChargeCameraCentering &&
		( TidePlayer->IsPlayingChargeDash() || TidePlayer->IsDashing() ) &&
		!TidePlayer->IsCameraInputActive();

	if ( !bCanAutoCenter ) return;

	const FRotator CurrentCamRot = ControlData.ControlRotation;
	const float PlayerYaw = TidePlayer->GetActorRotation().Yaw;

	const float YawDiff = FMath::FindDeltaAngleDegrees( CurrentCamRot.Yaw, PlayerYaw );

	// 差分が閾値（デッドゾーン）を超えている場合のみ追従させる
	if ( FMath::Abs( YawDiff ) <= TidePlayer->PlayerParamData->ChargeCameraCenteringThreshold ) return;

	FRotator TargetCamRot = CurrentCamRot;
	TargetCamRot.Yaw = PlayerYaw;

	// 閾値に近いときはゆっくり、離れるほど素早く追従させる
	const float SafeInterpWidth = FMath::Max( ChargeCameraCenteringInterpolationWidth, 1.0f );
	const float Alpha = FMath::Clamp( ( FMath::Abs( YawDiff ) - TidePlayer->PlayerParamData->ChargeCameraCenteringThreshold ) / SafeInterpWidth, 0.0f, 1.0f );

	const float DynamicSpeed = ChargeCameraCenteringSpeed * Alpha;

	InOutBaseRotation = FMath::RInterpTo( CurrentCamRot, TargetCamRot, PlayerDeltaTime, DynamicSpeed );

	if ( APlayerController* PC = Cast<APlayerController>( TidePlayer->GetController() ) )
	{
		PC->SetControlRotation( InOutBaseRotation );
	}
}

// 離陸時の高さ（GroundedFocusZ）を中心に、上は JumpFocusRiseDeadZone・下は JumpFocusFallDeadZone までは注視点を
// 動かさずプレイヤーを「置いていく」。超えたら縁に保つ高さへゆっくり追従し、内側へ戻れば元の高さへ戻す。
// 着地後は JumpFocusReconvergeSpeed で戻す。ロックオン追従中（縦構図を意図的に作る分岐）では適用しない
void UThirdPersonExCameraMode::UpdateJumpFocusVertical( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
	float PlayerDeltaTime, bool bIsLockOnFollowActive, FVector& InOutBaseFocusLocation )
{
	if ( bIsLockOnFollowActive )
	{
		// 追従中はその時の注視点Zを基準として保持し、解除直後のスナップを防ぐ
		CurrentFocusZ = InOutBaseFocusLocation.Z;
		GroundedFocusZ = ControlData.TargetLocation.Z;
		bPrevAirborne = false;	// 解除後の最初の空中で必ずアンカーを取り直す
		bFocusZInitialized = true;
	}
	else
	{
		const float PlayerZ = ControlData.TargetLocation.Z;

		if ( !bFocusZInitialized )
		{
			CurrentFocusZ = PlayerZ;
			GroundedFocusZ = PlayerZ;
			bFocusZInitialized = true;
		}

		const UTidePlayerParamDataAsset* Params = TidePlayer->PlayerParamData;
		const UCharacterMovementComponent* MoveComp = PlayerChar->GetCharacterMovement();

		// 連続幅跳び（チャージホップ）の間は接地しても空中として扱う。素直に接地判定へ落とすと再アンカーが
		// 着地のたびに走り、跳ぶたびに基準高さが上へずれて画が上下する（上り坂で顕著）
		const bool bHoldThroughHopChain = ( Params && Params->bJumpFocusHoldThroughChargeHop && TidePlayer->IsPlayingChargeHopJump() );
		const bool bAirborne = ( Params && Params->bEnableJumpFocusHold && MoveComp && ( MoveComp->IsFalling() || bHoldThroughHopChain ) );

		if ( !bAirborne )
		{
			// 接地中（または機能OFF）：注視点Zをプレイヤーへ追従させ、次ジャンプ用の基準高さも更新
			GroundedFocusZ = PlayerZ;
			const float ReconvergeSpeed = Params ? Params->JumpFocusReconvergeSpeed : 12.0f;
			CurrentFocusZ = FMath::FInterpTo( CurrentFocusZ, PlayerZ, PlayerDeltaTime, ReconvergeSpeed );
		}
		else
		{
			if ( !bPrevAirborne )
			{
				bExceededDeadZone = false;	// 接地→空中の遷移フレーム
			}

			// 離陸高さからの上下移動量（+ 上 / - 下）でデッドゾーン内外を判定
			const float Offset = PlayerZ - GroundedFocusZ;
			const float AbsOffset = FMath::Abs( Offset );

			// 上昇／落下で置いていく距離・追従速度を別パラメータにする（崖落ち等を独立調整するため）
			const bool bFalling = ( Offset < 0.0f );
			const float DirDeadZone = bFalling ? Params->JumpFocusFallDeadZone : Params->JumpFocusRiseDeadZone;
			const float DirInterpSpeed = bFalling ? Params->JumpFocusFallInterpSpeed : Params->JumpFocusAirborneInterpSpeed;

			if ( AbsOffset > DirDeadZone )
			{
				// 超過：プレイヤーをデッドゾーンの縁に保つ高さへ追従を開始する
				bExceededDeadZone = true;
				const float Sign = bFalling ? -1.0f : 1.0f;
				float TargetFocusZ = PlayerZ - Sign * DirDeadZone;

				// 高低差が開きすぎてフレームアウトしないよう安全クランプ（0以下で無制限）
				if ( Params->JumpFocusMaxVerticalOffset > 0.0f )
				{
					TargetFocusZ = FMath::Clamp( TargetFocusZ, PlayerZ - Params->JumpFocusMaxVerticalOffset, PlayerZ + Params->JumpFocusMaxVerticalOffset );
				}

				CurrentFocusZ = FMath::FInterpTo( CurrentFocusZ, TargetFocusZ, PlayerDeltaTime, DirInterpSpeed );
			}
			else if ( !bExceededDeadZone )
			{
				// デッドゾーン内：カメラは見ていた高さから動かさずプレイヤーを置いていく。即代入だと直前の
				// 着地から戻し切れていない差分が離陸の1フレームで消えて「ガクッ」となるため補間する
				const float HoldInterpSpeed = Params->JumpFocusHoldInterpSpeed;
				CurrentFocusZ = ( HoldInterpSpeed > 0.0f )
					? FMath::FInterpTo( CurrentFocusZ, GroundedFocusZ, PlayerDeltaTime, HoldInterpSpeed )
					: GroundedFocusZ;
			}
			else
			{
				// 一度超えてから内側へ戻ってきた：元の高さへ滑らかに戻す
				CurrentFocusZ = FMath::FInterpTo( CurrentFocusZ, GroundedFocusZ, PlayerDeltaTime, DirInterpSpeed );
			}
		}

		bPrevAirborne = bAirborne;
		InOutBaseFocusLocation.Z = CurrentFocusZ;

#if !UE_BUILD_SHIPPING
		if ( Params && UTideGameSettings::Get()->bDebugFlagDrawJumpFocus && GetWorld() )
		{
			DrawJumpFocusDebug( ControlData, PlayerChar, Params );
		}
#endif
	}

	// 空中で別カメラが Push されたとき新モードがこの値を種に高さを引き継げるよう、共有ストアへ書き戻す
	if ( UExCameraModeComponent* OwningComp = Cast<UExCameraModeComponent>( GetOuter() ) )
	{
		FJumpFocusVerticalState& Shared = OwningComp->GetJumpFocusVerticalState();
		Shared.bValid = true;
		Shared.CurrentFocusZ = CurrentFocusZ;
		Shared.GroundedFocusZ = GroundedFocusZ;
		Shared.bPrevAirborne = bPrevAirborne;
		Shared.bExceededDeadZone = bExceededDeadZone;
	}
}

void UThirdPersonExCameraMode::ApplyCameraLagAndClamp( float PlayerDeltaTime, const FRotator& BaseRotation, const FVector& BaseFocusLocation,
	bool bResetToFront, FRotator& OutSmoothedRotation, FVector& OutSmoothedFocusLocation )
{
	FRotator ClampedRotation = BaseRotation;
	ClampedRotation.Pitch = FMath::Clamp( FRotator::NormalizeAxis( ClampedRotation.Pitch ), CommonParams.PitchMin, CommonParams.PitchMax );
	if ( CommonParams.YawMin != CommonParams.YawMax )
	{
		ClampedRotation.Yaw = FMath::Clamp( FRotator::NormalizeAxis( ClampedRotation.Yaw ), CommonParams.YawMin, CommonParams.YawMax );
	}

	if ( PreviousFocusLocation.IsNearlyZero() )
	{
		PreviousFocusLocation = BaseFocusLocation;
		PreviousControlRotation = ClampedRotation;
	}

	bool bFinalEnableRotationLag = bEnableCameraRotationLag;
	bool bFinalEnableLagXY = bEnableCameraLagXY;
	bool bFinalEnableLagZ = bEnableCameraLagZ;

	if ( UTideGameSettings::Get()->bDebugFlagDisableLag )
	{
		bFinalEnableRotationLag = false;
		bFinalEnableLagXY = false;
		bFinalEnableLagZ = false;
	}

	// モード切替直後の初回フレーム：ラグの始点を理想値へ合わせて「収束済み」で開始する（詳細は OnActivated）。
	// 回転はここで、位置は UnlaggedFocusLocation 算出後に揃える
	if ( bPendingLagSeedFromIdeal )
	{
		PreviousControlRotation = ClampedRotation;
	}

	FRotator SmoothedRotation = ClampedRotation;
	if ( bFinalEnableRotationLag && !bResetToFront )
	{
		SmoothedRotation = FMath::RInterpTo( PreviousControlRotation, ClampedRotation, PlayerDeltaTime, CameraRotationLagSpeed );
	}
	PreviousControlRotation = SmoothedRotation;

	FRotator YawRotation( 0.0f, SmoothedRotation.Yaw, 0.0f );
	FVector LocalOffset = YawRotation.RotateVector( TargetOffset );

	// これは「回転軸（カメラが周回する中心）」の座標で、TargetOffset の Z が軸の高さを兼ねる。
	// 注視点は FocusHeightOffset（軸からの相対高さ）で別に持ち、呼び出し側が出力直前に向きを組み直す
	FVector UnlaggedFocusLocation = BaseFocusLocation + LocalOffset;

	if ( bPendingLagSeedFromIdeal )
	{
		PreviousFocusLocation = UnlaggedFocusLocation;
		bPendingLagSeedFromIdeal = false;
	}

	FVector SmoothedFocusLocation = UnlaggedFocusLocation;

	// XY（水平）と Z（垂直）のラグは独立して計算する（正面リセット時は補間せず即スナップ）
	if ( ( bFinalEnableLagXY || bFinalEnableLagZ ) && !bResetToFront )
	{
		if ( bFinalEnableLagXY )
		{
			const FVector2D PrevXY( PreviousFocusLocation.X, PreviousFocusLocation.Y );
			const FVector2D TargetXY( UnlaggedFocusLocation.X, UnlaggedFocusLocation.Y );
			const FVector2D SmoothedXY = FMath::Vector2DInterpTo( PrevXY, TargetXY, PlayerDeltaTime, CameraLagSpeedXY );
			SmoothedFocusLocation.X = SmoothedXY.X;
			SmoothedFocusLocation.Y = SmoothedXY.Y;
		}
		else
		{
			SmoothedFocusLocation.X = UnlaggedFocusLocation.X;
			SmoothedFocusLocation.Y = UnlaggedFocusLocation.Y;
		}

		if ( bFinalEnableLagZ )
		{
			SmoothedFocusLocation.Z = FMath::FInterpTo( PreviousFocusLocation.Z, UnlaggedFocusLocation.Z, PlayerDeltaTime, CameraLagSpeedZ );
		}
		else
		{
			SmoothedFocusLocation.Z = UnlaggedFocusLocation.Z;
		}

		// 最大距離のクランプは最終的な 3D ベクトル全体に対して行う
		if ( CameraLagMaxDistance > 0.0f )
		{
			const FVector FromTarget = SmoothedFocusLocation - UnlaggedFocusLocation;
			if ( FromTarget.SizeSquared() > FMath::Square( CameraLagMaxDistance ) )
			{
				SmoothedFocusLocation = UnlaggedFocusLocation + FromTarget.GetSafeNormal() * CameraLagMaxDistance;
			}
		}
	}

	PreviousFocusLocation = SmoothedFocusLocation;

	OutSmoothedRotation = SmoothedRotation;
	OutSmoothedFocusLocation = SmoothedFocusLocation;
}

FVector UThirdPersonExCameraMode::ResolveCameraCollision( const FVector& SweepStart, const FVector& IdealLocation, ACharacter* PlayerChar,
	float PlayerDeltaTime, bool bSnapCollision )
{
	FVector DesiredLocation = IdealLocation;

	if ( CollisionRadius <= 0.0f || !GetWorld() )
	{
		bCollisionPullInInitialized = false;
		return DesiredLocation;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( CameraSweep ), false );

	if ( PlayerChar )
	{
		QueryParams.AddIgnoredActor( PlayerChar );
		if ( LastLockedTargetComp.IsValid() )
		{
			QueryParams.AddIgnoredActor( LastLockedTargetComp->GetOwner() );
		}
	}

	while ( true )
	{
		const bool bHit = GetWorld()->SweepSingleByChannel(
			HitResult,
			SweepStart,
			DesiredLocation,
			FQuat::Identity,
			CollisionChannel,
			FCollisionShape::MakeSphere( CollisionRadius ),
			QueryParams
		);

		if ( !bHit )
		{
			break;
		}

		if ( AActor* HitActor = HitResult.GetActor() )
		{
			if ( HitActor->ActorHasTag( ExCameraSweepIgnoreTag ) )
			{
				QueryParams.AddIgnoredActor( HitActor );
				continue;
			}
		}

		if ( UPrimitiveComponent* HitComp = HitResult.GetComponent() )
		{
			if ( HitComp->ComponentHasTag( ExCameraSweepIgnoreTag ) )
			{
				if ( AActor* HitActor = HitComp->GetOwner() )
				{
					QueryParams.AddIgnoredActor( HitActor );
					continue;
				}
			}
		}

		DesiredLocation = HitResult.Location;
		break;
	}

	// スナップだと地形に触れた瞬間に手前へ飛ぶため寄せ量を補間する。位置を直接補間すると
	// 理想位置の移動（プレイヤーの移動・アーム長変化）まで鈍るので、寄せ量だけを持つ
	const FVector ToIdeal = IdealLocation - SweepStart;
	const float IdealDistance = ToIdeal.Size();
	if ( IdealDistance <= KINDA_SMALL_NUMBER )
	{
		bCollisionPullInInitialized = false;
		return DesiredLocation;
	}

	const float TargetPullIn = FMath::Clamp( IdealDistance - ( DesiredLocation - SweepStart ).Size(), 0.0f, IdealDistance );

	if ( bSnapCollision || !bCollisionPullInInitialized )
	{
		CurrentCollisionPullIn = TargetPullIn;
		bCollisionPullInInitialized = true;
	}
	else
	{
		// 寄る（当たった）／戻る（抜けた）で速度を分けられる。ともに 0 で即時
		const float InterpSpeed = ( TargetPullIn > CurrentCollisionPullIn ) ? CollisionPullInInterpSpeed : CollisionPullOutInterpSpeed;
		CurrentCollisionPullIn = FMath::Clamp( FMath::FInterpTo( CurrentCollisionPullIn, TargetPullIn, PlayerDeltaTime, InterpSpeed ), 0.0f, IdealDistance );
	}

	return SweepStart + ( ToIdeal / IdealDistance ) * ( IdealDistance - CurrentCollisionPullIn );
}

// ライド中かつロール追従ゾーン内なら、視線方向は保ったまま画面の上をプレイヤーの上方向へロールさせる
// （1周する筒でも操作と画面が一致する）
void UThirdPersonExCameraMode::ApplySurfaceRideRoll( ATidePlayerCharacter* TidePlayer, float PlayerDeltaTime, FRotator& InOutViewRotation )
{
	const bool bWantRoll = TidePlayer->IsSurfaceRiding() && TidePlayer->IsInSurfaceRideCameraRollZone();
	const float TargetAlpha = bWantRoll ? SurfaceRideRollMaxAlpha : 0.0f;
	CurrentSurfaceRideRollAlpha = FMath::FInterpTo( CurrentSurfaceRideRollAlpha, TargetAlpha, PlayerDeltaTime, SurfaceRideRollInterpSpeed );

	if ( CurrentSurfaceRideRollAlpha <= KINDA_SMALL_NUMBER ) return;

	const FVector ViewForward = InOutViewRotation.Vector();
	const FVector DesiredUp = TidePlayer->GetSurfaceRideUp();

	// 視線と目標アップが平行に近いとロール基準が定まらない
	if ( FMath::Abs( FVector::DotProduct( ViewForward, DesiredUp ) ) >= 0.985f ) return;

	const FQuat NoRollQuat = InOutViewRotation.Quaternion();										// 現状（ワールド上基準）
	const FQuat RolledQuat = FRotationMatrix::MakeFromXZ( ViewForward, DesiredUp ).ToQuat();		// 上をプレイヤーの上へ
	InOutViewRotation = FQuat::Slerp( NoRollQuat, RolledQuat, CurrentSurfaceRideRollAlpha ).Rotator();
}

#if !UE_BUILD_SHIPPING
void UThirdPersonExCameraMode::DrawJumpFocusDebug( const FCameraControlData& ControlData, ACharacter* PlayerChar, const UTidePlayerParamDataAsset* Params ) const
{
	const FVector PlayerLoc = ControlData.TargetLocation;
	const FVector Side = PlayerChar->GetActorRightVector() * 70.0f;	// プレイヤーの真横へずらす
	const FVector Fwd = PlayerChar->GetActorForwardVector() * 40.0f;	// 横線の長さ（前後方向）

	// 支点には最後に TargetOffset.Z が足されるので、線もその高さに合わせる
	const float ViewZOffset = TargetOffset.Z;
	auto DrawLevel = [&]( float Z, const FColor& Color )
	{
		const FVector C = FVector( PlayerLoc.X, PlayerLoc.Y, Z + ViewZOffset ) + Side;
		DrawDebugLine( GetWorld(), C - Fwd, C + Fwd, Color, false, -1.0f, 0, 2.0f );
	};

	const float TakeoffZ = GroundedFocusZ;
	const float DeadZoneTopZ = GroundedFocusZ + Params->JumpFocusRiseDeadZone;
	const float DeadZoneBottomZ = GroundedFocusZ - Params->JumpFocusFallDeadZone;

	DrawLevel( TakeoffZ, FColor::Green );			// 緑：離陸高さ
	DrawLevel( DeadZoneTopZ, FColor::Red );			// 赤：上限（超えると画面上方へ抜ける）
	DrawLevel( DeadZoneBottomZ, FColor::Magenta );	// 紫：下限（崖落ち等で超えると追従開始）
	DrawLevel( CurrentFocusZ, FColor::Cyan );		// 水色：現在の注視点Z
	DrawLevel( PlayerLoc.Z, FColor::Yellow );		// 黄：現在のプレイヤー高さ

	// 下限〜上限をつなぐ縦の支柱
	const FVector Pillar0 = FVector( PlayerLoc.X, PlayerLoc.Y, DeadZoneBottomZ + ViewZOffset ) + Side;
	const FVector Pillar1 = FVector( PlayerLoc.X, PlayerLoc.Y, DeadZoneTopZ + ViewZOffset ) + Side;
	DrawDebugLine( GetWorld(), Pillar0, Pillar1, FColor::Orange, false, -1.0f, 0, 1.5f );
}

// 緑＝回転軸（SmoothedFocusLocation）／水色＝注視点／黄＝カメラ位置（スウィープ後）／
// マゼンタ＝アーム（カメラ→回転軸）／白＝アクター原点（腰基準。軸がどれだけ上がっているかの比較用）
void UThirdPersonExCameraMode::DrawPivotFocusDebug( const FCameraControlData& ControlData, const FVector& PivotLocation, const FVector& CameraLocation, float LookAtDeltaZ ) const
{
	const FVector PivotLoc = PivotLocation;											// 回転軸
	const FVector FocusLoc = PivotLocation + FVector( 0.0f, 0.0f, LookAtDeltaZ );	// 注視点
	const FVector CamLoc = CameraLocation;
	const FVector OriginLoc = ControlData.TargetLocation;							// アクター原点（腰基準）

	DrawDebugSphere( GetWorld(), PivotLoc, 12.0f, 12, FColor::Green, false, -1.0f, 0, 1.5f );
	DrawDebugSphere( GetWorld(), FocusLoc, 10.0f, 12, FColor::Cyan, false, -1.0f, 0, 1.5f );
	DrawDebugSphere( GetWorld(), CamLoc, 8.0f, 8, FColor::Yellow, false, -1.0f, 0, 1.5f );
	DrawDebugSphere( GetWorld(), OriginLoc, 6.0f, 8, FColor::White, false, -1.0f, 0, 1.0f );

	DrawDebugLine( GetWorld(), CamLoc, PivotLoc, FColor::Magenta, false, -1.0f, 0, 1.5f );	// アーム
	DrawDebugLine( GetWorld(), CamLoc, FocusLoc, FColor::Cyan, false, -1.0f, 0, 1.0f );		// 視線
	DrawDebugLine( GetWorld(), OriginLoc, PivotLoc, FColor::White, false, -1.0f, 0, 1.0f );	// 原点→回転軸の高さ差

	// DrawDebugString のキャンバスフォントは日本語グリフを持たないためラベルは ASCII で描く
	DrawDebugString( GetWorld(), PivotLoc + FVector( 0.0f, 0.0f, 15.0f ), TEXT( "Pivot" ), nullptr, FColor::Green, 0.0f );
	DrawDebugString( GetWorld(), FocusLoc + FVector( 0.0f, 0.0f, -18.0f ), TEXT( "Focus" ), nullptr, FColor::Cyan, 0.0f );
}
#endif

void UThirdPersonExCameraMode::OnActivated( const FMinimalViewInfo& LastViewInfo )
{
	// ラグの始点は「直前ビューからの逆算（位置＋視線×アーム長）」では作らない。逆算はアーム長の違うモード間や
	// 注視点傾き込みの出力回転で真の回転軸からずれ、切替直後にラグがそれを追う「お辞儀／スウープ」になる。
	// モード間のなめらかさは Component 側のブレンドが担保するので、初回 UpdateCamera で収束済みから始める
	bPendingLagSeedFromIdeal = true;
	bCollisionPullInInitialized = false;	// 壁避けの寄せ量も同様

	// 仮想追従座標だけは概算（直前ビューのアーム長先）で種付けする（対象継続時のスナップ防止）
	const FVector CamDirection = LastViewInfo.Rotation.Vector();
	CurrentTrackingTargetLoc = LastViewInfo.Location + ( CamDirection * TargetArmLength );
	CurrentFocusOffset = FVector::ZeroVector;
	CurrentPullBackDistance = 0.0f;

	// 縦“置いていき”状態を前のカメラから引き継ぐ。共有ストアの値を種にして再アンカーしないため、
	// 空中で別カメラが Push されても保持中の高さが継続して縦に飛ばない
	bool bSeededFromShared = false;
	if ( const UExCameraModeComponent* OwningComp = Cast<UExCameraModeComponent>( GetOuter() ) )
	{
		const FJumpFocusVerticalState& Shared = OwningComp->GetJumpFocusVerticalState();
		if ( Shared.bValid )
		{
			CurrentFocusZ = Shared.CurrentFocusZ;
			GroundedFocusZ = Shared.GroundedFocusZ;
			bPrevAirborne = Shared.bPrevAirborne;
			bExceededDeadZone = Shared.bExceededDeadZone;
			bFocusZInitialized = true;	// 再アンカーしない（プレイヤー現在高さへスナップさせない）
			bSeededFromShared = true;
		}
	}

	// 共有ストアが無効（起動直後など）のときだけ、次フレームでプレイヤー高さから初期化する
	if ( !bSeededFromShared )
	{
		bFocusZInitialized = false;
		bPrevAirborne = false;
		bExceededDeadZone = false;
	}
}
