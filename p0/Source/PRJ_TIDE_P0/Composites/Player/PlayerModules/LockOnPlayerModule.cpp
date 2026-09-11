// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "LockOnPlayerModule.h"

#include <imgui.h>
#include "Engine/OverlapResult.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"

void ULockOnPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );

	if ( ULockOnComponent* LockOnComp = OwnerCharacter->FindComponentByClass<ULockOnComponent>() )
	{
		LockOnComp->SearchRadius = OwnerCharacter->PlayerParamData->CanLockOnDistance;
	}
}

void ULockOnPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	// 発動専有中はロックオン状態・カメラを凍結し、自動ロスト解除が演出カメラのスタックを乱さないようにする
	// （構え選択中は演出カメラがまだ無いので通常どおり動かす）
	if ( OwnerCharacter->IsGodActionExecuting() ) return;

	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp || !LockOnComp->HasTarget() ) return;

	UpdateLockOnCondition( DeltaTime );
	DrawLockOnMarkerDebugUI( DeltaTime );

	// --- 以下は強制振り向きの禁止条件 ---
	if( OwnerCharacter->IsDodging() ) return;
	if ( OwnerCharacter->IsDashing() ) return;	// ダッシュ中は自由に旋回できるように
	if ( OwnerCharacter->IsPlayingChargeAction() ) return;

	// 落下ループ中は IsPlayingChargeAction() が false になり自動振り向きが復活するため、
	// 着地まで true の IsPlayingChargeHopJump() で別途弾く
	if ( OwnerCharacter->IsPlayingChargeHopJump() ) return;

	ULockOnTargetComponent* TargetComp = LockOnComp->GetTarget();
	if ( !TargetComp ) return;

	FVector DirectionToTarget = TargetComp->GetTargetLocation() - OwnerCharacter->GetActorLocation();
	DirectionToTarget.Z = 0.0f;

	if ( !DirectionToTarget.IsNearlyZero() )
	{
		const FRotator TargetRot = DirectionToTarget.Rotation();
		const FRotator CurrentRot = OwnerCharacter->GetActorRotation();

		float TurnRate = OwnerCharacter->GetCurrentRotationRateYaw();
		if ( TurnRate > UE_KINDA_SMALL_NUMBER )
		{
			const FRotator NewRot = FMath::RInterpConstantTo( CurrentRot, TargetRot, DeltaTime, TurnRate );
			OwnerCharacter->SetActorRotation( NewRot );
		}
	}
}

void ULockOnPlayerModule::ToggleLockOn()
{
	if ( !OwnerCharacter ) return;

	// 発動専有中はトグル（＝カメラの push/pop）を受け付けない。演出カメラの上に別カメラが積まれると
	// pop 時にブレンドされずハードカットになる
	if ( OwnerCharacter->IsGodActionExecuting() ) return;

	auto* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp ) return;

	LockOnComp->ToggleLockOn();

	if( LockOnComp->HasTarget() )
	{
		OnStartLockOn();
	}
	else
	{
		OnEndLockOn();
	}

	OwnerCharacter->RefreshMovementParams();
}

bool ULockOnPlayerModule::IsLockOnActive() const
{
	if ( !OwnerCharacter ) return false;
	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	return LockOnComp && LockOnComp->HasTarget();
}

void ULockOnPlayerModule::UpdateLockOnCondition( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp ) return;

	ULockOnTargetComponent* TargetComp = LockOnComp->GetTarget();
	if ( !TargetComp )
	{
		CurrentLostTimer.Clear();
		return;
	}

	// 条件付き部位の解禁終了・死亡などで不可になったら即解除する
	if ( !TargetComp->bIsTargetable )
	{
		ClearTarget();
		return;
	}

	const float MaxLockOnDistance = OwnerCharacter->PlayerParamData->MaxLockOnDistance;
	const float TimeToLoseLockOn = OwnerCharacter->PlayerParamData->TimeToLoseLockOn;

	const FVector OwnerLoc = OwnerCharacter->GetActorLocation();
	const FVector TargetLoc = TargetComp->GetTargetLocation();

	const float DistanceSquared = FVector::DistSquared( OwnerLoc, TargetLoc );
	if ( DistanceSquared > FMath::Square( MaxLockOnDistance ) )
	{
		ClearTarget();
		return;
	}

	// 遮蔽判定はカメラ基準で行う
	FVector CameraLoc = OwnerLoc;	// 取れない場合のフォールバック
	if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
	{
		if ( PC->PlayerCameraManager )
		{
			CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( LockOnLOS ), false, OwnerCharacter );

	bool bHit = OwnerCharacter->GetWorld()->LineTraceSingleByChannel(
		HitResult,
		CameraLoc,
		TargetLoc,
		ECC_Visibility,
		QueryParams
	);

	bool bIsOccluded = false;
	if ( bHit && HitResult.GetActor() != TargetComp->GetOwner() )
	{
		bIsOccluded = true;
	}

	if ( bIsOccluded )
	{
		CurrentLostTimer.Update( DeltaTime );
		if ( CurrentLostTimer.IsFinish() )
		{
			ClearTarget();
			return;
		}
	}
	else
	{
		CurrentLostTimer.Set( TimeToLoseLockOn );
	}
}

void ULockOnPlayerModule::ClearTarget()
{
	if ( !OwnerCharacter ) return;

	if ( ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent() )
	{
		LockOnComp->ClearLockOn();
	}

	OnEndLockOn();
}

void ULockOnPlayerModule::OnStartLockOn()
{
	if ( !OwnerCharacter ) return;

	// 開始直前のカメラ操作が IsCameraInputActive() に残っていると「手動操作中」分岐に入って対象へ追従せず、
	// 遅延ぶん経ってから急に追従し始めてカクつく
	OwnerCharacter->ResetCameraInputActivity();

	// 起動しながら調整できるよう毎回設定する
	if ( ULockOnComponent* LockOnComp = OwnerCharacter->FindComponentByClass<ULockOnComponent>() )
	{
		LockOnComp->SearchRadius = OwnerCharacter->PlayerParamData->CanLockOnDistance;
	}

	if ( !LockOnCameraHandle.IsValid() )
	{
		if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					LockOnCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( TEXT( "SimpleLockOn" ), TEXT( "SimpleLockOn" ) );
				}
			}
		}
	}
}

void ULockOnPlayerModule::OnEndLockOn()
{
	if ( LockOnCameraHandle.IsValid() )
	{
		if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					// bForceFixedBlendStart=true で退場側のカメラを開始時の構図に固定し、
					// 対象を失った退場カメラが注視点を一気に中央へ戻す（差分追従の吸い込み）のを防ぐ
					if ( CameraSubsystem->PopCameraMode( LockOnCameraHandle, TEXT( "SimpleLockOn" ), /*bForceFixedBlendStart=*/true ) )
					{
						LockOnCameraHandle.Clear();
					}
				}
			}
		}
	}
}

bool ULockOnPlayerModule::TryProcessMovementInput( float Right, float Forward, const FVector2D& ClampedInput )
{
	if ( !OwnerCharacter ) return false;

	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp || !LockOnComp->HasTarget() )
	{
		return false;	// 通常の移動を行わせる
	}

	AController* Controller = OwnerCharacter->GetController();
	if ( !Controller ) return false;

	// カニ歩き移動
	const FRotator CameraRot = Controller->GetControlRotation();
	const FRotator CameraYawRot( 0.0, CameraRot.Yaw, 0.0 );
	const FVector CameraForward = FRotationMatrix( CameraYawRot ).GetUnitAxis( EAxis::X );
	const FVector CameraRight = FRotationMatrix( CameraYawRot ).GetUnitAxis( EAxis::Y );

	OwnerCharacter->AddMovementInput( CameraForward, ClampedInput.Y );
	OwnerCharacter->AddMovementInput( CameraRight, ClampedInput.X );

	return true;	// モジュール側で移動を処理した
}

void ULockOnPlayerModule::RequestTargetSwitch( float InputValue )
{
	if ( !IsLockOnActive() ) return;

	const bool bLookRight = InputValue > 0.0f;

	if ( ULockOnTargetComponent* NextTarget = FindSwitchCandidate( bLookRight ) )
	{
		if ( ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent() )
		{
			// 直接上書きする（Pop/Push は行わないので滑らかに遷移する）
			LockOnComp->SetTarget( NextTarget );
		}
	}
}

ULockOnTargetComponent* ULockOnPlayerModule::FindSwitchCandidate( bool bIsRight ) const
{
	if ( !OwnerCharacter ) return nullptr;

	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp ) return nullptr;

	ULockOnTargetComponent* CurrentTarget = LockOnComp->GetTarget();
	if ( !CurrentTarget ) return nullptr;

	APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
	if ( !PC || !PC->PlayerCameraManager ) return nullptr;

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector CameraRight = FRotationMatrix( PC->PlayerCameraManager->GetCameraRotation() ).GetUnitAxis( EAxis::Y );

	// LockOnComponent の検索範囲で周囲の敵を集める
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	for ( auto Type : LockOnComp->TargetObjectTypes )
	{
		ObjectParams.AddObjectTypesToQuery( UEngineTypes::ConvertToCollisionChannel( Type ) );
	}

	OwnerCharacter->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		OwnerCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere( LockOnComp->SearchRadius )
	);

	ULockOnTargetComponent* BestCandidate = nullptr;
	float BestScore = -MAX_flt;

	const FVector DirToCurrent = ( CurrentTarget->GetTargetLocation() - CameraLoc ).GetSafeNormal();

	for ( const FOverlapResult& Result : Overlaps )
	{
		AActor* CandidateActor = Result.GetActor();
		if ( !CandidateActor || CandidateActor == OwnerCharacter ) continue;

		TArray<ULockOnTargetComponent*> TargetComps;
		CandidateActor->GetComponents<ULockOnTargetComponent>( TargetComps );

		for ( ULockOnTargetComponent* Candidate : TargetComps )
		{
			if ( !Candidate || Candidate == CurrentTarget || !Candidate->bIsTargetable ) continue;

			const FVector CandidateLoc = Candidate->GetTargetLocation();
			const FVector DirToCandidate = ( CandidateLoc - CameraLoc ).GetSafeNormal();

			if ( FVector::DotProduct( CameraForward, DirToCandidate ) < LockOnComp->MinDotProductThreshold ) continue;

			const float DotToRight = FVector::DotProduct( CameraRight, DirToCandidate );
			const float CurrentTargetDotToRight = FVector::DotProduct( CameraRight, DirToCurrent );

			const bool bIsCorrectSide = bIsRight ? ( DotToRight > CurrentTargetDotToRight ) : ( DotToRight < CurrentTargetDotToRight );
			if ( !bIsCorrectSide ) continue;

			const float SimilarityScore = FVector::DotProduct( DirToCurrent, DirToCandidate );

			if ( SimilarityScore > BestScore )
			{
				BestScore = SimilarityScore;
				BestCandidate = Candidate;
			}
		}
	}

	return BestCandidate;
}

void ULockOnPlayerModule::DrawLockOnMarkerDebugUI( float DeltaTime )
{
	if ( !OwnerCharacter ) return;

	ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent();
	if ( !LockOnComp ) return;

	ULockOnTargetComponent* TargetComp = LockOnComp->GetTarget();
	if ( !TargetComp ) return;

	APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
	if ( !PC ) return;

	// TODO: データ化する
	constexpr float TargetZOffset = 50.0f;
	constexpr float ScaleRate = 2.0f;

	constexpr float RotationSpeed = 1.0f;
	constexpr float PulseSpeed = 5.0f;

	constexpr float CenterDotRadius = 1.0f * ScaleRate;
	constexpr float InnerCircleRadius = 6.0f * ScaleRate;
	constexpr float InnerCircleThickness = 1.0f * ScaleRate;

	constexpr float OuterLineInnerRadius = 8.0f * ScaleRate;
	constexpr float OuterLineOuterRadius = 12.0f * ScaleRate;
	constexpr float OuterLineThickness = 1.0f * ScaleRate;

	const ImVec4 BaseRed = ImVec4( 1.0f, 0.3f, 0.3f, 1.0f );
	const ImVec4 DarkRed = ImVec4( 0.7f, 0.1f, 0.1f, 1.0f );

	const ImU32 ShadowColor = IM_COL32( 200, 200, 200, 255 );
	constexpr float ShadowThicknessOffset = 1.5f;	// 輪郭線としてメインの線に加える太さ

	const FVector TargetLoc = TargetComp->GetTargetLocation() + FVector( 0.0f, 0.0f, TargetZOffset );
	FVector2D ScreenPos;
	if ( !PC->ProjectWorldLocationToScreen( TargetLoc, ScreenPos ) ) return;

	ImVec2 Center( ScreenPos.X, ScreenPos.Y );
	ImDrawList* DrawList = ImGui::GetBackgroundDrawList();

	LockOnMarkerRotation += DeltaTime * RotationSpeed;
	if ( LockOnMarkerRotation > PI * 2.0f ) LockOnMarkerRotation -= PI * 2.0f;

	const float TimeSec = GetWorld()->GetTimeSeconds();
	const float PulseAlpha = 0.3f + ( FMath::Sin( TimeSec * PulseSpeed ) * 0.5f + 0.5f ) * 0.7f;

	const ImU32 CoreColor = ImGui::ColorConvertFloat4ToU32( DarkRed );
	const ImU32 PulseColor = ImGui::ColorConvertFloat4ToU32( ImVec4( DarkRed.x, DarkRed.y, DarkRed.z, PulseAlpha ) );
	const ImU32 ShadowPulseColor = ImGui::ColorConvertFloat4ToU32( ImVec4( 200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, PulseAlpha ) );
	const ImU32 BGColor = ImGui::ColorConvertFloat4ToU32( DarkRed );

	// 以下いずれも「輪郭色の太線 → 本体色の線」の順に重ねて縁取りする

	// 中央のコア（ドット）
	DrawList->AddCircleFilled( Center, CenterDotRadius + ShadowThicknessOffset, ShadowColor );
	DrawList->AddCircleFilled( Center, CenterDotRadius, CoreColor );

	// 内側の固定円
	DrawList->AddCircle( Center, InnerCircleRadius, ShadowColor, 32, InnerCircleThickness + ShadowThicknessOffset );
	DrawList->AddCircle( Center, InnerCircleRadius, BGColor, 32, InnerCircleThickness );

	// 回転する外枠
	constexpr int32 NumOuterLines = 3;
	constexpr float AngleStep = ( PI * 2.0f ) / NumOuterLines;

	for ( int32 i = 0; i < NumOuterLines; ++i )
	{
		const float Angle = LockOnMarkerRotation + ( i * AngleStep );
		const ImVec2 Dir( FMath::Cos( Angle ), FMath::Sin( Angle ) );

		const ImVec2 P1 = ImVec2( Center.x + Dir.x * OuterLineInnerRadius, Center.y + Dir.y * OuterLineInnerRadius );
		const ImVec2 P2 = ImVec2( Center.x + Dir.x * OuterLineOuterRadius, Center.y + Dir.y * OuterLineOuterRadius );

		DrawList->AddLine( P1, P2, ShadowPulseColor, OuterLineThickness + ShadowThicknessOffset );
		DrawList->AddLine( P1, P2, PulseColor, OuterLineThickness );
	}
}

#if !UE_BUILD_SHIPPING
void ULockOnPlayerModule::DrawDebugImGui()
{
	if ( !OwnerCharacter ) return;

	ImGui::Indent();
	{
		ImGui::BulletText( "LockOnPlayerModule情報" );

		ImGui::Indent();
		{
			const bool bIsActive = IsLockOnActive();
			ImGui::Text( "モジュール状態:" );
			ImGui::SameLine();
			if ( bIsActive )
			{
				ImGui::TextColored( ImVec4( 0.2f, 1.0f, 0.2f, 1.0f ), "アクティブ" );
			}
			else
			{
				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "待機中" );
			}

			if ( bIsActive )
			{
				ImGui::Spacing();

				ImGui::Text( "カメラハンドル:" );
				ImGui::SameLine();
				if ( LockOnCameraHandle.IsValid() )
				{
					ImGui::TextColored( ImVec4( 0.2f, 1.0f, 0.2f, 1.0f ), "有効 (ID: %d)", LockOnCameraHandle.ID );
				}
				else
				{
					ImGui::TextColored( ImVec4( 1.0f, 0.2f, 0.2f, 1.0f ), "無効" );
				}

				ImGui::Separator();

				if ( OwnerCharacter->PlayerParamData )
				{
					ImGui::Text( "【設定パラメータ】" );
					ImGui::BulletText( "最大ロックオン距離: %.1f", OwnerCharacter->PlayerParamData->MaxLockOnDistance );
					ImGui::BulletText( "ロスト猶予時間: %.2f 秒", OwnerCharacter->PlayerParamData->TimeToLoseLockOn );
				}

				ImGui::Spacing();

				ImGui::Text( "【遮蔽判定】" );
				if ( CurrentLostTimer.IsFinish() )
				{
					ImGui::TextColored( ImVec4( 1.0f, 0.2f, 0.2f, 1.0f ), "ロスト" );
				}
				else
				{
					const float Duration = OwnerCharacter->PlayerParamData->TimeToLoseLockOn;
					const float Elapsed = CurrentLostTimer.GetElapsed();
					const float Ratio = FMath::Clamp( Elapsed / FMath::Max( Duration, 0.001f ), 0.0f, 1.0f );

					if ( Ratio > 0.0f )
					{
						ImGui::TextColored( ImVec4( 1.0f, 0.6f, 0.0f, 1.0f ), "遮蔽中... (%.2f / %.2f)", Elapsed, Duration );
						ImGui::ProgressBar( Ratio, ImVec2( -1.0f, 0.0f ) );
					}
					else
					{
						ImGui::TextColored( ImVec4( 0.2f, 1.0f, 1.0f, 1.0f ), "視界クリア" );
					}
				}
			}
		}
		ImGui::Unindent();

		ImGui::Separator();

		ImGui::BulletText( "LockOnComponent情報" );

		if ( ULockOnComponent* LockOnComp = OwnerCharacter->GetLockOnComponent() )
		{
			LockOnComp->DrawImGuiDebug();
		}

	}
	ImGui::Unindent();
}
#endif
