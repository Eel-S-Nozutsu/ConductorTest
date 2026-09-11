// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodSlashSubModule.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Misc/App.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionPlayerModule.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Companion/TideGodBird.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

// 星型巡回順のステップ幅算出に使う
static int32 GreatestCommonDivisorInt( int32 A, int32 B )
{
	while ( B != 0 )
	{
		const int32 T = B;
		B = A % B;
		A = T;
	}
	return A;
}

// AttackTypeTag + GearIndex が一致する行（無ければ nullptr）。
// 一閃固有の自己完結ヘルパにしているのはユニティビルドでの static 名衝突を避けるため
static const FPlayerAttackParameterRow* FindGodSlashRow( const ATidePlayerCharacter* Player, int32 GearIndex )
{
	if ( !Player ) return nullptr;
	const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
	if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

	static const FString Context = TEXT( "PlayerAttackParamLookup" );
	TArray<FPlayerAttackParameterRow*> Rows;
	DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );

	for ( const FPlayerAttackParameterRow* Row : Rows )
	{
		if ( Row && Row->AttackTypeTag == TAG_AttackType_Player_GodActionSlash && Row->GearLevel == GearIndex )
		{
			return Row;
		}
	}
	return nullptr;
}

// --- mover アクセサ（切り抜けを走らせる本体。既定＝プレイヤー、戯れ＝神鳥）---

AActor* UGodSlashSubModule::GetMover() const
{
	if ( AActor* Mover = MoverActor.Get() )
	{
		return Mover;
	}
	return OwnerCharacter;
}

bool UGodSlashSubModule::IsMoverCharacter() const
{
	const AActor* Player = OwnerCharacter;
	return GetMover() == Player;
}

FVector UGodSlashSubModule::GetMoverLocation() const
{
	const AActor* Mover = GetMover();
	return Mover ? Mover->GetActorLocation() : FVector::ZeroVector;
}

FRotator UGodSlashSubModule::GetMoverRotation() const
{
	const AActor* Mover = GetMover();
	return Mover ? Mover->GetActorRotation() : FRotator::ZeroRotator;
}

void UGodSlashSubModule::SetMoverLocation( const FVector& NewLocation )
{
	if ( AActor* Mover = GetMover() )
	{
		// プレイヤー本体は床すり抜け対策で TeleportPhysics（不連続移動）が必要。鳥 mover は TeleportPhysics だと
		// 毎フレーム瞬間移動と見なされリボン系トレイルがリセットされるため、コリジョン無しを活かし通常移動で動かす
		const ETeleportType Teleport = IsMoverCharacter() ? ETeleportType::TeleportPhysics : ETeleportType::None;
		Mover->SetActorLocation( NewLocation, false, nullptr, Teleport );
	}
}

void UGodSlashSubModule::SetMoverRotation( const FRotator& NewRotation )
{
	if ( AActor* Mover = GetMover() )
	{
		Mover->SetActorRotation( NewRotation );
	}
}

void UGodSlashSubModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 実時間（FApp）駆動でダイレーション非依存に一定の長さにする
	UpdateCutSlow();

	switch ( CurrentState )
	{
	case EGodSlashState::LockingOn:
		UpdateLockingOn( DeltaTime );
		break;
	case EGodSlashState::Slashing:
		UpdateSlashing( DeltaTime );
		break;
	default:
		break;
	}
}

// --- 入力要求 ---

void UGodSlashSubModule::RequestLockOnStart()
{
	// ゲージの有無は問わない（実行時に消費する）
	if ( CurrentState == EGodSlashState::Idle && CanActivate() )
	{
		OnStartLockOn();
	}
}

void UGodSlashSubModule::RequestLockOnRelease()
{
	// Idle / Slashing 中の解除は無視する（B キャンセル後の解除もここで弾かれる）
	if ( CurrentState != EGodSlashState::LockingOn ) return;

	PruneInvalidTargets();
	if ( OwnerModule && OwnerModule->IsGaugeFull() && LockedTargets.Num() > 0 )
	{
		OnExecuteSlash();
	}
	else
	{
		ClearAction( true );	// ゲージ不足・対象無しはキャンセル扱い（ゲージは消費しない）
	}
}

void UGodSlashSubModule::RequestCancel()
{
	// 発動が確定する前なのでゲージは消費しない
	if ( CurrentState == EGodSlashState::LockingOn )
	{
		ClearAction( true );
	}
}

void UGodSlashSubModule::ExecuteAsFrolic( AActor* Mover )
{
	// 発動判定は RequestLockOnRelease と同じ（満タン＆対象ありで発動）
	if ( CurrentState != EGodSlashState::LockingOn ) return;

	PruneInvalidTargets();
	if ( !( OwnerModule && OwnerModule->IsGaugeFull() && LockedTargets.Num() > 0 ) )
	{
		ClearAction( true );
		return;
	}

	// Mover が無効ならプレイヤー本体のまま（＝憑依と同じ挙動）。鳥は外部駆動モードにして
	// 自前の追従・浮遊を止め、一閃ロジックが位置を直接動かせるようにする
	MoverActor = Mover;
	bForceWideCut = true;
	if ( ATideGodBird* Bird = Cast<ATideGodBird>( Mover ) )
	{
		Bird->SetExternallyDriven( true );
	}

	// 戻り〜ED でこの正反対（背を向ける向き）へ鳥を向ける
	bHasFrolicCameraFacing = false;
	if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
	{
		const FRotator CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
		FrolicCameraYaw = CamRot.Yaw;
		bHasFrolicCameraFacing = true;
	}

	OnExecuteSlash();
}

// --- ロックオン（一閃）---

bool UGodSlashSubModule::CanActivate() const
{
	if ( !OwnerCharacter ) return false;
	if ( OwnerCharacter->IsDead() ) return false;
	if ( OwnerCharacter->IsHitReacting() ) return false;
	return true;
}

void UGodSlashSubModule::OnStartLockOn()
{
	if ( !OwnerCharacter ) return;

	// 発動時の性能はこの値で決まる
	ActivatedGearIndex = OwnerCharacter->GetCurrentChargeGearIndex();

	// 構え入りでは進行中アクションを止めない。ただし未消費の突風バフだけは破棄する
	// （次のチャージダッシュで突風バーストが暴発しないように）
	OwnerCharacter->ClearGustChargeBuff();

	CurrentState = EGodSlashState::LockingOn;

	LockedTargets.Reset();
	LockOnMontageTimer.Clear();
	LockOnScanTimer.Clear();

	RefreshLockedTargets();

	// 構えモーション（GOD_SLASH_ST→LP）は傘 UGodActionPlayerModule が神技別に駆動する
	// （憑依のみ一連で再生／戯れ・導きは再生しない）。ここはターゲット走査のみを担う
}

void UGodSlashSubModule::UpdateLockingOn( float DeltaTime )
{
	// 一定間隔でターゲットを再取得する（近づいた敵の追加・離れた敵の除去）
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	LockOnScanTimer.Update( DeltaTime );
	if ( !LockOnScanTimer.IsValid() || LockOnScanTimer.IsFinish() )
	{
		RefreshLockedTargets();
		LockOnScanTimer.Set( OwnerCharacter->PlayerParamData->GodSlashLockOnScanInterval );
	}
}

void UGodSlashSubModule::RefreshLockedTargets()
{
	if ( !OwnerCharacter ) return;
	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	PruneInvalidTargets();

	const int32 MaxCount = GetMaxLockOnCountForGear();
	if ( LockedTargets.Num() >= MaxCount ) return;

	const float MaxDist = GetLockOnDistanceForGear();
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	TSet<ULockOnTargetComponent*> AlreadyLocked;	// 重複追加の防止用
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		if ( Weak.IsValid() ) AlreadyLocked.Add( Weak.Get() );
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor( OwnerCharacter );
	World->OverlapMultiByChannel(
		Overlaps, MyLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere( MaxDist ), QueryParams );

	// 候補は LockOnTargetComponent 単位＝部位ごとに 1 ロックオン（重複なし）
	struct FCandidate
	{
		ULockOnTargetComponent* Comp = nullptr;
		float Score = 0.0f;
	};
	TArray<FCandidate> Candidates;

	const float MaxDistSq = FMath::Square( MaxDist );

	for ( const FOverlapResult& Result : Overlaps )
	{
		AActor* Actor = Result.GetActor();
		if ( !Actor || Actor == OwnerCharacter ) continue;

		TArray<ULockOnTargetComponent*> TargetComps;
		Actor->GetComponents<ULockOnTargetComponent>( TargetComps );

		for ( ULockOnTargetComponent* Comp : TargetComps )
		{
			if ( !Comp || !Comp->bIsTargetable ) continue;
			// OverlapMultiByChannel は同一アクターを複数回返すため重複を弾く
			if ( AlreadyLocked.Contains( Comp ) ) continue;

			const FVector TargetLoc = Comp->GetTargetLocation();
			const float DistSq = FVector::DistSquared( MyLoc, TargetLoc );
			if ( DistSq > MaxDistSq ) continue;

			// 画面（レティクル）内に収まっている対象のみロックオンする
			if ( !IsTargetOnScreen( TargetLoc ) ) continue;

			if ( !IsTargetVisibleFromCamera( TargetLoc ) )
			{
				// AlreadyLocked には入れない（次スキャンで再評価させる）
				continue;
			}

			// 近いほど・優先度が高いほど高スコア
			const float DistRate = 1.0f - FMath::Sqrt( DistSq ) / MaxDist;
			Candidates.Add( { Comp, DistRate + Comp->TargetPriorityScore } );
			AlreadyLocked.Add( Comp );
		}
	}

	Candidates.Sort( []( const FCandidate& A, const FCandidate& B ) { return A.Score > B.Score; } );

	for ( const FCandidate& Candidate : Candidates )
	{
		if ( LockedTargets.Num() >= MaxCount ) break;
		LockedTargets.Add( Candidate.Comp );
	}
}

void UGodSlashSubModule::PruneInvalidTargets()
{
	if ( !OwnerCharacter )
	{
		LockedTargets.Reset();
		return;
	}

	const float MaxDist = GetLockOnDistanceForGear();
	const float MaxDistSq = FMath::Square( MaxDist );
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	LockedTargets.RemoveAll( [&]( const TWeakObjectPtr<ULockOnTargetComponent>& Weak )
	{
		ULockOnTargetComponent* Comp = Weak.Get();
		if ( !Comp || !Comp->bIsTargetable ) return true;
		const FVector TargetLoc = Comp->GetTargetLocation();
		if ( FVector::DistSquared( MyLoc, TargetLoc ) > MaxDistSq ) return true;
		// 画面（レティクル）外に出た対象はロックオンから外す
		if ( !IsTargetOnScreen( TargetLoc ) ) return true;
		return false;
	} );
}

bool UGodSlashSubModule::IsTargetVisibleFromCamera( const FVector& TargetLocation ) const
{
	if ( !OwnerCharacter ) return false;

	if ( OwnerCharacter->PlayerParamData && !OwnerCharacter->PlayerParamData->bGodSlashRequireLineOfSight )
	{
		return true;
	}

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return true;

	// カメラ基準で遮蔽を見る（フォールバックはプレイヤーの目線）
	FVector ViewLoc;
	FRotator ViewRot;
	OwnerCharacter->GetActorEyesViewPoint( ViewLoc, ViewRot );
	if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
	{
		if ( PC->PlayerCameraManager )
		{
			ViewLoc = PC->PlayerCameraManager->GetCameraLocation();
		}
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodSlashLOS ), false, OwnerCharacter );

	const bool bHit = World->LineTraceSingleByChannel(
		Hit, ViewLoc, TargetLocation, ECC_Visibility, QueryParams );

	if ( !bHit ) return true;

	// 遮蔽物が Pawn なら地形ではないので可視扱い（マルチロックオンを壊さないため）
	if ( const UPrimitiveComponent* HitComp = Hit.GetComponent() )
	{
		if ( HitComp->GetCollisionObjectType() == ECC_Pawn ) return true;
	}

	return false;
}

bool UGodSlashSubModule::IsTargetOnScreen( const FVector& TargetLocation ) const
{
	if ( !OwnerCharacter ) return false;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	const float FracX = Params ? Params->GodSlashLockOnScreenFractionX : 0.8f;
	const float FracY = Params ? Params->GodSlashLockOnScreenFractionY : 0.7f;

	// どちらかでも 0 以下なら画面内フィルタを無効化（全方位ロックオン）
	if ( FracX <= 0.0f || FracY <= 0.0f ) return true;

	APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() );
	if ( !PC ) return true;	// PC が取れない場合はフィルタしない

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize( SizeX, SizeY );
	if ( SizeX <= 0 || SizeY <= 0 ) return true;

	// カメラ背後・画面外への投影は false が返る → 画面外扱い
	FVector2D ScreenPos;
	if ( !PC->ProjectWorldLocationToScreen( TargetLocation, ScreenPos, false ) )
	{
		return false;
	}

	// 中央基準で、画面サイズの Frac 割の矩形（＝大型レティクル）内に収まっているか
	const float MarginX = SizeX * ( 1.0f - FracX ) * 0.5f;
	const float MarginY = SizeY * ( 1.0f - FracY ) * 0.5f;

	return ScreenPos.X >= MarginX && ScreenPos.X <= ( SizeX - MarginX )
		&& ScreenPos.Y >= MarginY && ScreenPos.Y <= ( SizeY - MarginY );
}

// --- 一閃発動 ---

void UGodSlashSubModule::OnExecuteSlash()
{
	if ( !OwnerCharacter ) return;

	// 既定は一閃1回で全消費。デバッグフラグ ON のときだけロック対象数／最大数の割合だけ消費して温存する
	float GaugeConsumeFraction = 1.0f;
	if ( const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData )
	{
		if ( Params->bDebugGodActionPartialGaugeConsume )
		{
			const int32 MaxLockCount = GetMaxLockOnCountForGear();
			const int32 UsedLockCount = LockedTargets.Num();
			GaugeConsumeFraction = ( MaxLockCount > 0 )
				? FMath::Clamp( (float)UsedLockCount / (float)MaxLockCount, 0.0f, 1.0f )
				: 1.0f;
		}
	}
	if ( OwnerModule )
	{
		OwnerModule->ConsumeGauge( GaugeConsumeFraction );
	}

	// ActivatedGearIndex は OnStartLockOn で確定済み。
	// この時点では既にチャージがリセットされているため取得し直さない

	// 発動中にパラメータが変わっても揺れないようここで確定する。戯れ（鳥 mover）は強制 ON——
	// カメラが発動時 TPS 姿勢基準の固定ショットで mover を参照しないため流用できる
	bWideCutActive = bForceWideCut
		|| ( OwnerCharacter->PlayerParamData && OwnerCharacter->PlayerParamData->bGodSlashWideCutPresentation );

	if ( bWideCutActive )
	{
		BuildWideCutStarOrder();
		WideCutStepCounter = 0;
		WideCutLoopElapsed = 0.0f;
	}
	else if ( LockedTargets.Num() > 1 )
	{
		// クラシック演出は斬り上げにする。斬る順は LockedTargets の格納順なので、
		// Z 昇順に並べれば一番低い対象から始まり一番高い対象が最後の締めになる
		LockedTargets.Sort( []( const TWeakObjectPtr<ULockOnTargetComponent>& A, const TWeakObjectPtr<ULockOnTargetComponent>& B )
		{
			const ULockOnTargetComponent* CompA = A.Get();
			const ULockOnTargetComponent* CompB = B.Get();
			if ( !CompA ) return false;
			if ( !CompB ) return true;
			return CompA->GetTargetLocation().Z < CompB->GetTargetLocation().Z;
		} );
	}

	CurrentState = EGodSlashState::Slashing;
	SlashPhase = ESlashPhase::Stance;
	CurrentSlashIndex = 0;
	bFinishBlownOff = false;

	LockOnMontageTimer.Clear();
	LockOnScanTimer.Clear();

	SlashStartLocation = GetMoverLocation();	// Return フェーズで戻る先
	CameraFocusLocation = OwnerCharacter->GetActorLocation();

	// 残留速度を消し、重力に縛られず三次元的に斬りかかれるよう飛行モードにする。鳥 mover でもプレイヤーが
	// 空中なら落下しないよう飛行にして滞空させる（地上の戯れは待機のままでよい）
	if ( IsMoverCharacter() || IsAirborneDuringSlash() )
	{
		OwnerCharacter->StopVelocity();
		if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
		{
			Movement->SetMovementMode( MOVE_Flying );
		}
	}

	// カメラだけ先に Push する。リードイン（ATK ST）は通常速度で見せ、
	// スローはその後の斬りかかり移行時に BeginSlashSlowMotion で開始する
	StartSlashStaging();

	const float Duration = PlayAnimMontage( PlayerAnimTags::GOD_SLASH_ATK_ST );
	SlashTimer.Set( Duration > 0.0f ? Duration : 0.1f );
}

void UGodSlashSubModule::UpdateSlashing( float DeltaTime )
{
	switch ( SlashPhase )
	{
	case ESlashPhase::Stance:
	{
		// リードイン（ATK ST）を通常速度で待つ。この間はまだ斬りかからず、スローもかけない
		SlashTimer.Update( DeltaTime );
		if ( !SlashTimer.IsFinish() ) return;

		BeginSlashSlowMotion();	// 切り抜けから世界がスローになる

		if ( LockedTargets.Num() > 0 )
		{
			SlashPhase = ESlashPhase::Loop;
			CurrentSlashIndex = bWideCutActive ? GetWideCutTargetIndexForStep( WideCutStepCounter ) : 0;
			BeginSlashOnTarget( CurrentSlashIndex );
		}
		else
		{
			OnEnterSlashEnd();
		}
		break;
	}

	case ESlashPhase::Loop:
	{
		// ワイドカットは切り抜け専用の高速移動を使う（クラシックの GodSlashMoveSpeed とは独立）。
		// 経過を積算し、GodSlashWideCutLoopDuration に達したら周回を打ち切る
		const float ZigzagSpeed = bWideCutActive ? GetWideCutZigzagMoveSpeed() : -1.0f;
		if ( bWideCutActive )
		{
			WideCutLoopElapsed += DeltaTime;
			WideCutLpMontageTimer.Update( DeltaTime );

			// 目標向きと角速度は BeginSlashOnTarget が区間の所要時間に同期して更新する。
			// RInterpConstantTo（一定角速度）なので、折り返しでも移動と同じペースで向き終える
			if ( WideCutFacingAngularSpeed > 0.0f )
			{
				const FRotator NewFacing = FMath::RInterpConstantTo( GetMoverRotation(), WideCutDesiredFacing, DeltaTime, WideCutFacingAngularSpeed );
				SetMoverRotation( NewFacing );
			}
			else
			{
				SetMoverRotation( WideCutDesiredFacing );
			}
		}

		// 静止（dwell）は入れず、貫通時のスローで移動が遅くなることで“貫通の溜め”を見せつつ常に動き続ける
		const bool bReached = MoveTowardSlashTarget( DeltaTime, ZigzagSpeed );
		SlashTimer.Update( DeltaTime );

		// SlashMoveTarget は対象の PassThrough だけ奥にあるため、残距離が PassThrough 以下＝対象中心に到達
		// とみなす（手前ふちへ前倒しするなら CutHitLeadRadiusScale で広げる）
		if ( !bCutHitFired )
		{
			const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
			const float PassThrough = Params ? Params->GodSlashPassThroughDistance : 250.0f;
			const float LeadScale = Params ? Params->GodSlashCutHitLeadRadiusScale : 0.0f;

			float Threshold = PassThrough;
			if ( LeadScale > 0.0f )
			{
				FVector DummyCenter;
				Threshold += GetTargetMeshRadius( GetTargetAt( CurrentSlashIndex ), DummyCenter ) * LeadScale;
			}

			const float DistToMoveTargetSq = FVector::DistSquared( GetMoverLocation(), SlashMoveTarget );
			if ( bReached || SlashTimer.IsFinish() || DistToMoveTargetSq <= Threshold * Threshold )
			{
				// ワイドカットは貫通ごとの手応えを挟まず、途切れない移動として見せる
				if ( !bWideCutActive )
				{
					OnCutHitTarget( CurrentSlashIndex );
				}
				bCutHitFired = true;
			}
		}

		if ( bReached || SlashTimer.IsFinish() )
		{
			// ワイドカットは到達のたびに速度をゼロへ戻さない——StopVelocity は Velocity をリセットするため、
			// それを参照するアニメーション（傾き・移動ブレンド）が対象ごとにニュートラルへ戻って止まって見える
			if ( !bWideCutActive && IsMoverCharacter() )
			{
				OwnerCharacter->StopVelocity();
			}
			AdvanceToNextSlashTarget();
		}
		break;
	}

	case ESlashPhase::Return:
		UpdateReturning( DeltaTime );
		break;

	case ESlashPhase::End:
	default:
		SlashTimer.Update( DeltaTime );

		// 一斉吹っ飛び後（余韻中）のみ CanMove での早期キャンセルを許可する
		// （吹っ飛び前にキャンセルされると敵がダメージを受けないため）
		if ( bFinishBlownOff && OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) )
		{
			const bool bAir = IsAirborneDuringSlash();
			const bool bGodBird = OwnerModule && OwnerModule->IsGodBirdModeEnabled();

			// 通常モードの空中は CanMove 付与で即・落下ループへ引き継ぐ
			if ( bAir && !bGodBird )
			{
				ClearAction( true );
				OwnerCharacter->EnterJumpFallingLoop();
				return;
			}
			// 地上と神鳥検証モードの空中は、移動入力があるときだけ余韻をキャンセルする
			if ( !OwnerCharacter->GetRawMovementInput().IsNearlyZero( 0.1f ) )
			{
				ClearAction( true );
				if ( bAir )
				{
					OwnerCharacter->EnterJumpFallingLoop();
				}
				return;
			}
		}

		if ( SlashTimer.IsFinish() )
		{
			if ( !bFinishBlownOff )
			{
				// タメ終了 → 一斉吹っ飛び
				bFinishBlownOff = true;

				if ( OwnerCharacter->PlayerParamData->bGodSlashRestoreTimeOnFinish )
				{
					RestoreSlashSlowMotion();
				}

				ApplyFinishDamageToAllTargets();

				const float Hold = OwnerCharacter->PlayerParamData->GodSlashFinishHoldTime;
				SlashTimer.Set( Hold > 0.0f ? Hold : 0.01f );
			}
			else
			{
				// 神鳥検証モードの空中は、ED モーションが終わるまで飛行のまま滞空して待つ
				// （落下ステートへ移ると ED が途切れて見えるため）
				const bool bGodBird = OwnerModule && OwnerModule->IsGodBirdModeEnabled();
				bool bMontagePlaying = false;
				if ( const USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh() )
				{
					if ( const UAnimInstance* Anim = Mesh->GetAnimInstance() )
					{
						bMontagePlaying = Anim->IsAnyMontagePlaying();
					}
				}

				// 万一 ED が終わらない場合に滞空し続けないよう上限を設ける
				constexpr float MaxAirEndWait = 2.5f;
				if ( bGodBird && IsAirborneDuringSlash() && bMontagePlaying && EndAirWaitElapsed < MaxAirEndWait )
				{
					EndAirWaitElapsed += DeltaTime;
					SlashTimer.Set( 0.05f );	// ED 再生中は飛行滞空のまま小刻みに再チェック
				}
				else
				{
					OnEndSlash();
				}
			}
		}
		break;
	}
}

void UGodSlashSubModule::AdvanceToNextSlashTarget()
{
	if ( bWideCutActive )
	{
		// 星型巡回順を GodSlashWideCutLoopDuration まで何周でも繰り返す（同じ対象群を何度も往復する見せ方）
		const float LoopDuration = OwnerCharacter->PlayerParamData
			? OwnerCharacter->PlayerParamData->GodSlashWideCutLoopDuration : 1.2f;
		if ( WideCutLoopElapsed < LoopDuration )
		{
			++WideCutStepCounter;
			CurrentSlashIndex = GetWideCutTargetIndexForStep( WideCutStepCounter );
			BeginSlashOnTarget( CurrentSlashIndex );
		}
		else
		{
			BeginReturning();
		}
		return;
	}

	++CurrentSlashIndex;
	if ( CurrentSlashIndex < LockedTargets.Num() )
	{
		BeginSlashOnTarget( CurrentSlashIndex );
	}
	else
	{
		OnEnterSlashEnd();
	}
}

void UGodSlashSubModule::BeginReturning()
{
	SlashPhase = ESlashPhase::Return;

	if ( !OwnerCharacter ) return;

	SlashMoveTarget = SlashStartLocation;

	const float ReturnSpeed = OwnerCharacter->PlayerParamData
		? OwnerCharacter->PlayerParamData->GodSlashWideCutReturnMoveSpeed : 10000.0f;

	// 詰まって到達できない場合に長期化しないための上限（距離/速度＋余裕）
	const float Dist = FVector::Dist( GetMoverLocation(), SlashMoveTarget );
	const float EstimatedTime = Dist / FMath::Max( ReturnSpeed, 1.0f ) + 0.2f;
	SlashTimer.Set( EstimatedTime );
}

void UGodSlashSubModule::UpdateReturning( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData )
	{
		OnEnterSlashEnd();
		return;
	}

	const float ReturnSpeed = OwnerCharacter->PlayerParamData->GodSlashWideCutReturnMoveSpeed;
	const bool bReached = MoveTowardSlashTarget( DeltaTime, ReturnSpeed );
	SlashTimer.Update( DeltaTime );

	if ( bReached || SlashTimer.IsFinish() )
	{
		if ( IsMoverCharacter() )
		{
			OwnerCharacter->StopVelocity();
		}

		// 鳥 mover は発動時カメラ Yaw の正反対で締める（メッシュ補正込み）。
		// プレイヤー mover は切り抜けた対象群へ背を向ける
		if ( ATideGodBird* Bird = ( !IsMoverCharacter() && bHasFrolicCameraFacing ) ? Cast<ATideGodBird>( GetMover() ) : nullptr )
		{
			Bird->SetActorFacingWithModelOffset( FRotator( 0.0f, FrolicCameraYaw + 180.0f, 0.0f ) );
		}
		else
		{
			FVector GroupCenter;
			float GroupRadius;
			GetLockedTargetsGroupBounds( GroupCenter, GroupRadius );

			FVector AwayDir = GetMoverLocation() - GroupCenter;
			AwayDir.Z = 0.0f;
			AwayDir = AwayDir.GetSafeNormal();
			if ( !AwayDir.IsNearlyZero() )
			{
				SetMoverRotation( AwayDir.Rotation() );
			}
		}

		OnEnterSlashEnd();
	}
}

void UGodSlashSubModule::BeginSlashOnTarget( int32 TargetIndex )
{
	// ワイドカットは前回の再生が終わってからのみ再生し直す
	// （毎回リトリガーするとブレンドインが終わらず巻き戻り続け、モーションだけ止まって見える）
	if ( !bWideCutActive || !WideCutLpMontageTimer.IsValid() || WideCutLpMontageTimer.IsFinish() )
	{
		const float LpDuration = PlayAnimMontage( PlayerAnimTags::GOD_SLASH_ATK_LP );
		if ( bWideCutActive )
		{
			WideCutLpMontageTimer.Set( LpDuration > 0.0f ? LpDuration : 0.2f );
		}
	}

	if ( !OwnerCharacter ) return;

	bCutHitFired = false;

	// 1 対象あたりの移動保険（届かない場合に次へ進むため）
	const float MaxTime = OwnerCharacter->PlayerParamData
		? OwnerCharacter->PlayerParamData->GodSlashMaxTimePerTarget : 0.4f;
	SlashTimer.Set( MaxTime > 0.0f ? MaxTime : 0.4f );

	ULockOnTargetComponent* Comp = GetTargetAt( TargetIndex );
	if ( !Comp )
	{
		SlashMoveTarget = GetMoverLocation();
		return;
	}

	// ワープせず UpdateSlashing で高速移動する。ワイドカットは実位置ではなく重心基準の「星型の頂点」を目的地にする
	const FVector RawTargetLoc = Comp->GetTargetLocation();
	const FVector TargetLoc = bWideCutActive ? GetWideCutStarWaypoint( RawTargetLoc, WideCutStepCounter ) : RawTargetLoc;
	const FVector FromLoc = GetMoverLocation();

	CameraFocusLocation = TargetLoc;	// 演出カメラの注視先

	const float PassThroughDistance = OwnerCharacter->PlayerParamData
		? OwnerCharacter->PlayerParamData->GodSlashPassThroughDistance : 250.0f;

	// 三次元方向（上下含む）で対象を通り越した位置まで進む。空中の対象にも届き、敵をある程度貫いてから次へ
	FVector Dir3D = ( TargetLoc - FromLoc ).GetSafeNormal();
	if ( Dir3D.IsNearlyZero() )
	{
		SlashMoveTarget = FromLoc;
		return;
	}
	SlashMoveTarget = TargetLoc + Dir3D * PassThroughDistance;

	// 保険タイマーと向き補間速度の両方で使い、区間ごとに実際の移動ペースへ同期させる
	float EstimatedLegDuration = 0.0f;
	if ( bWideCutActive )
	{
		const float LegDistance = FVector::Dist( FromLoc, SlashMoveTarget );
		const float ZigzagSpeed = GetWideCutZigzagMoveSpeed();
		EstimatedLegDuration = FMath::Max( LegDistance / FMath::Max( ZigzagSpeed, 1.0f ), 0.01f );

		// クラシック向けの GodSlashMaxTimePerTarget（既定 0.4 秒）をそのまま保険にすると、到達判定が成立しない
		// 区間（星型頂点が地面下でクランプされ続ける等）で最大 0.4 秒「詰まって見える」
		SlashTimer.Set( EstimatedLegDuration + 0.05f );
	}

	// 振り向きは Yaw のみ（水平方向）。真上・真下の場合は向きを維持
	FVector PlanarDir = ( TargetLoc - FromLoc );
	PlanarDir.Z = 0.0f;
	PlanarDir = PlanarDir.GetSafeNormal();
	if ( !PlanarDir.IsNearlyZero() )
	{
		if ( bWideCutActive )
		{
			// 瞬時スナップにせず目標向きだけ更新し、Loop で毎フレーム補間する。角速度はこの区間の所要時間
			// ちょうどで向き終わるよう逆算する——固定値だと小さな調整に合わせれば折り返しで回頭が追いつかず、
			// 折り返しに合わせれば普段が無駄に速く見える
			WideCutDesiredFacing = PlanarDir.Rotation();

			const float AngleDelta = FMath::Abs( ( WideCutDesiredFacing - GetMoverRotation() ).GetNormalized().Yaw );
			const float SpeedMultiplier = OwnerCharacter->PlayerParamData
				? FMath::Max( 0.0f, OwnerCharacter->PlayerParamData->GodSlashWideCutFacingSpeedMultiplier ) : 1.3f;
			WideCutFacingAngularSpeed = ( AngleDelta / EstimatedLegDuration ) * SpeedMultiplier;
		}
		else
		{
			SetMoverRotation( PlanarDir.Rotation() );
		}
	}
}

bool UGodSlashSubModule::MoveTowardSlashTarget( float DeltaTime, float MoveSpeedOverride )
{
	if ( !OwnerCharacter ) return true;

	const float MoveSpeed = ( MoveSpeedOverride > 0.0f )
		? MoveSpeedOverride
		: ( OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->GodSlashMoveSpeed : 8000.0f );

	const FVector CurLoc = GetMoverLocation();

	constexpr float ArriveThreshold = 30.0f;
	if ( FVector::DistSquared( CurLoc, SlashMoveTarget ) <= ArriveThreshold * ArriveThreshold )
	{
		return true;
	}

	// 敵はすり抜けて切り込むイメージなので sweep なし
	FVector NextLoc = FMath::VInterpConstantTo( CurLoc, SlashMoveTarget, DeltaTime, MoveSpeed );

	// 敵はすり抜けるが地形（床）には潜らせない。移動前位置と実際に使った速度も渡し、高速な垂直降下で
	// トレース開始点を追い越したり、スイープの深度が足りず貫通するのを防ぐ
	NextLoc = ClampLocationAboveGround( CurLoc, NextLoc, MoveSpeed );

	SetMoverLocation( NextLoc );

	return FVector::DistSquared( NextLoc, SlashMoveTarget ) <= ArriveThreshold * ArriveThreshold;
}

namespace
{
	// Pawn を含めない＝敵をすり抜けつつ静的／動的どちらの床にも乗れる。ByChannel(ECC_WorldStatic) だと
	// Pawn の敵カプセルも Block して敵を地面と誤検出し、WorldStatic だけだと床が WorldDynamic のとき貫通する
	FCollisionObjectQueryParams MakeGroundObjectQuery()
	{
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery( ECC_WorldStatic );
		ObjectParams.AddObjectTypesToQuery( ECC_WorldDynamic );
		return ObjectParams;
	}
}

FVector UGodSlashSubModule::ClampLocationAboveGround( const FVector& FromLocation, const FVector& DesiredLocation, float MoveSpeedOverride ) const
{
	if ( !OwnerCharacter ) return DesiredLocation;

	// 鳥 mover はカプセルを持たず地面へ潜っても問題ない。
	// 下振れは GetWideCutStarWaypoint 側の真下トレースで既に抑えてある
	if ( !IsMoverCharacter() ) return DesiredLocation;

	UWorld* World = OwnerCharacter->GetWorld();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if ( !World || !Capsule ) return DesiredLocation;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float Clearance = Params ? Params->GodSlashGroundClearance : 2.0f;
	const float MoveSpeed = ( MoveSpeedOverride > 0.0f )
		? MoveSpeedOverride
		: ( Params ? Params->GodSlashMoveSpeed : 8000.0f );

	// 床面の検出は線トレースではなく「カプセルを真下へスイープ」で行う（線は XY 1 点しか見ないため、薄い床・端・
	// 高速移動で取りこぼす）。スイープ開始点は移動前位置と移動先の高い方＋余白から取る——1 フレームの移動量は
	// カプセル半分を超えるため、移動先だけを基準にすると高速降下で開始点が地面より下に潜って床を拾えない
	const float TopZ = FMath::Max( FromLocation.Z, DesiredLocation.Z ) + HalfHeight + 50.0f;
	const float Reach = HalfHeight + Clearance + FMath::Max( MoveSpeed * 0.05f, 300.0f );

	const FVector SweepStart( DesiredLocation.X, DesiredLocation.Y, TopZ );
	const FVector SweepEnd( DesiredLocation.X, DesiredLocation.Y, DesiredLocation.Z - Reach );

	FHitResult Hit;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodSlashGroundClamp ), false, OwnerCharacter );
	// 地形（WorldStatic＋WorldDynamic）のみ。敵 Pawn は除外（MakeGroundObjectQuery 参照）
	const FCollisionObjectQueryParams ObjectParams = MakeGroundObjectQuery();
	const FCollisionShape CapShape = FCollisionShape::MakeCapsule( Radius, HalfHeight );

	if ( !World->SweepSingleByObjectType( Hit, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, CapShape, QueryParams )
		|| Hit.bStartPenetrating )
	{
		return DesiredLocation;
	}

	// Hit.Location は床の上にカプセルがちょうど載る中心座標。クリアランスぶん浮かせて
	// 「これ以上は潜らせない中心 Z」を作り、移動先がそれより下なら押し上げる
	const float MinCenterZ = Hit.Location.Z + Clearance;
	FVector Result = DesiredLocation;
	if ( Result.Z < MinCenterZ )
	{
		Result.Z = MinCenterZ;
	}
	return Result;
}

bool UGodSlashSubModule::IsAirborneDuringSlash() const
{
	if ( !OwnerCharacter ) return false;

	UWorld* World = OwnerCharacter->GetWorld();
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if ( !World || !Capsule ) return false;

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	// 足元からこの距離内に地形があれば接地とみなす
	constexpr float GroundTolerance = 12.0f;

	const FVector Center = OwnerCharacter->GetActorLocation();
	const FVector TraceEnd = Center - FVector( 0.0f, 0.0f, HalfHeight + GroundTolerance );

	FHitResult Hit;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodSlashAirborne ), false, OwnerCharacter );
	const FCollisionObjectQueryParams ObjectParams = MakeGroundObjectQuery();	// 敵 Pawn は踏んでも接地扱いにしない

	const bool bGround = World->LineTraceSingleByObjectType(
		Hit, Center, TraceEnd, ObjectParams, QueryParams );
	return !bGround;
}

void UGodSlashSubModule::OnEndSlash()
{
	const bool bAirborne = IsAirborneDuringSlash();
	const bool bGodBird = OwnerModule && OwnerModule->IsGodBirdModeEnabled();

	if ( bAirborne && !bGodBird )
	{
		// 通常モードの空中：モンタージュを止めて JumpModule の落下ループへ引き継ぐ
		ClearAction( true );
		if ( OwnerCharacter )
		{
			OwnerCharacter->EnterJumpFallingLoop();
		}
	}
	else
	{
		// 地上、または神鳥検証モードの空中：ED を自然に終わらせる（モンタージュを止めない）
		ClearAction( false );
	}
}

void UGodSlashSubModule::ClearAction( bool bStopMontage )
{
	// プレイヤーの等速復帰は EndSlashStaging→RestoreSlashSlowMotion が担うため、ここでは触らない
	EndCutSlow( false );

	EndSlashStaging();

	if ( bStopMontage && OwnerCharacter )
	{
		OwnerCharacter->StopAnimMontage();
	}

	// 専有が完全に終了するこのタイミングで、接地状態に応じたステートへ戻す
	if ( OwnerCharacter )
	{
		if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
		{
			if ( Movement->MovementMode == MOVE_Flying )
			{
				if ( IsAirborneDuringSlash() )
				{
					Movement->SetMovementMode( MOVE_Falling );
				}
				else
				{
					// Walking に戻すと AnimBP の着地モーションをバイパスして即座に地上 Locomotion へ入る
					Movement->SetMovementMode( MOVE_Walking );
				}
			}
		}

		OwnerCharacter->RefreshMovementParams();
	}

	// 鳥へ差し替えていた場合は外部駆動を解除して追従・浮遊へ戻す。次回以降の一閃は再びプレイヤー本体が mover になる
	if ( ATideGodBird* Bird = Cast<ATideGodBird>( MoverActor.Get() ) )
	{
		Bird->CustomTimeDilation = 1.0f;	// スロー未開始でのキャンセル等の保険
		Bird->SetExternallyDriven( false );
	}
	MoverActor = nullptr;
	bForceWideCut = false;
	bHasFrolicCameraFacing = false;

	CurrentState = EGodSlashState::Idle;
	SlashPhase = ESlashPhase::Stance;
	CurrentSlashIndex = 0;
	bFinishBlownOff = false;
	bCutHitFired = false;
	bWideCutActive = false;
	WideCutStarOrder.Reset();
	WideCutStepCounter = 0;
	WideCutLoopElapsed = 0.0f;
	WideCutFacingAngularSpeed = 0.0f;
	WideCutLpMontageTimer.Clear();

	LockedTargets.Reset();
	LockOnMontageTimer.Clear();
	LockOnScanTimer.Clear();
	SlashTimer.Clear();
}

// --- 一閃の演出（スロー・カメラ・カットヒット・ED 一斉ダメージ）---

void UGodSlashSubModule::OnEnterSlashEnd()
{
	SlashPhase = ESlashPhase::End;
	bFinishBlownOff = false;
	EndAirWaitElapsed = 0.0f;

	if ( OwnerCharacter )
	{
		OwnerCharacter->StopVelocity();

		UWorld* World = OwnerCharacter->GetWorld();
		const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();

		if ( World && Capsule )
		{
			const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const float Clearance = Params ? Params->GodSlashGroundClearance : 2.0f;
			const float PassThrough = Params ? Params->GodSlashPassThroughDistance : 250.0f;

			// 移動中は床クランプしていないため終了時に地面へ深くめり込んでいる場合がある。真下トレースでは自分より
			// 上の床面を拾えないので、十分上空（SearchUp＝想定最大めり込み深度）からスイープして接地座標を求める
			const FVector Base = OwnerCharacter->GetActorLocation();
			const float SearchUp = HalfHeight + Clearance + PassThrough + 100.0f;
			const float SearchDown = HalfHeight + Clearance + 200.0f;
			const FVector SweepStart = Base + FVector( 0.0f, 0.0f, SearchUp );
			const FVector SweepEnd = Base - FVector( 0.0f, 0.0f, SearchDown );

			FHitResult Hit;
			FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodSlashGroundSnap ), false, OwnerCharacter );
			const FCollisionObjectQueryParams ObjectParams = MakeGroundObjectQuery();	// 敵 Pawn に乗り上げてスナップしない
			FCollisionShape CapShape = Capsule->GetCollisionShape();

			if ( World->SweepSingleByObjectType( Hit, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, CapShape, QueryParams )
				&& !Hit.bStartPenetrating )
			{
				const FVector GroundedLoc = Hit.Location;	// 床の上にカプセルがちょうど載る中心座標（斜面でも正確）

				// 正当な滞空（空中の対象を斬った直後など）は接地させず落下へ任せる。
				// 床と同高度かそれより下に潜っているときだけ、床上へ載せて Walking へ移行する
				constexpr float GroundSnapTolerance = 50.0f;
				if ( Base.Z <= GroundedLoc.Z + GroundSnapTolerance )
				{
					OwnerCharacter->SetActorLocation( GroundedLoc, false, nullptr, ETeleportType::TeleportPhysics );

					if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
					{
						Movement->SetMovementMode( MOVE_Walking );
					}
				}
			}
		}
	}

	PlayAnimMontage( PlayerAnimTags::GOD_SLASH_ATK_ED );

	// カメラが正面へ回り込むタメ。満了で一斉吹っ飛びを適用する
	const float Windup = OwnerCharacter && OwnerCharacter->PlayerParamData
		? OwnerCharacter->PlayerParamData->GodSlashFinishWindupTime : 0.35f;
	SlashTimer.Set( Windup > 0.0f ? Windup : 0.01f );
}

void UGodSlashSubModule::OnCutHitTarget( int32 TargetIndex )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	ULockOnTargetComponent* Comp = GetTargetAt( TargetIndex );
	if ( !Comp ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	const FVector HitLoc = Comp->GetTargetLocation();

	// 実ダメージ・吹っ飛びは ED の一斉適用まで保留し、ここは手応えの見た目だけ
	if ( OwnerCharacter->NiagaraSystemDataAsset )
	{
		if ( UNiagaraSystem* Effect = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::HIT_MEDIUM ) )
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				OwnerCharacter->GetWorld(), Effect, HitLoc, FRotator::ZeroRotator, FVector( 1.0f ) );
		}
	}

	// 貫いた瞬間、0 ダメージで小ノックバックのリアクションだけ与える
	if ( AActor* TargetActor = Comp->GetOwner() )
	{
		if ( IDamageable* Damageable = Cast<IDamageable>( TargetActor ) )
		{
			FDamageInfo Info;
			Info.Instigator = OwnerCharacter;
			Info.AttackTypeTag = TAG_AttackType_Player_GodActionSlash;
			Info.BaseDamage = 0.0f;
			Info.HitReactionTag = TAG_HitReaction_GodSkill_Iai;

			FHitResult Hit;
			Hit.Location = HitLoc;
			Hit.ImpactPoint = HitLoc;
			Info.HitResult = Hit;

			Damageable->ReceiveDamage( Info );
		}
	}

	// 完全停止ではなく「プレイヤーと斬った敵を一緒にスロー」で余韻を出す。両者へ同じ CustomTimeDilation を
	// 当てると、神技スロー（ワールド側）の上で二者だけがさらに遅くなり周囲から浮いた“溜め”になる
	if ( Params->bGodSlashCutSlow )
	{
		BeginCutSlow( Comp->GetOwner() );
	}
	else if ( const FPlayerAttackParameterRow* Row = FindGodSlashRow( OwnerCharacter, ActivatedGearIndex ) )
	{
		// 斬られた敵にだけヒットストップ
		if ( Row->bUseHitStop && Row->HitStopDuration > 0.0f )
		{
			if ( AActor* TargetActor = Comp->GetOwner() )
			{
				HitStopUtil::ApplyHitStop( TargetActor, Row->HitStopDuration, Row->HitStopDilation );
			}
		}
	}
}

float UGodSlashSubModule::GetSlashPlayerDilation() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return 1.0f;
	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;
	// BeginSlashSlowMotion と同じ：ワールドを GlobalDilation で遅くし、プレイヤーは等速側へ補正
	const float GlobalDilation = FMath::Clamp( Params->GodSlashGlobalTimeDilation, 0.01f, 1.0f );
	return Params->GodSlashPlayerTimeScale / GlobalDilation;
}

void UGodSlashSubModule::BeginCutSlow( AActor* Enemy )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 既にスロー中なら前の敵を元の倍率へ戻してから貼り直す（連続カットの取りこぼし防止）
	if ( bCutSlowActive )
	{
		if ( AActor* Prev = CutSlowEnemy.Get() )
		{
			Prev->CustomTimeDilation = CutSlowEnemyBaseDilation;
		}
	}

	CutSlowEnemy = Enemy;
	CutSlowEnemyBaseDilation = Enemy ? Enemy->CustomTimeDilation : 1.0f;
	CutSlowElapsed = 0.0f;
	bCutSlowActive = true;
	// 実際の倍率設定は UpdateCutSlow のイージングで行う。In 開始＝基準速度なので初回は変化なし
}

void UGodSlashSubModule::UpdateCutSlow()
{
	if ( !bCutSlowActive ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) { EndCutSlow( false ); return; }

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	// FApp はダイレーション非依存。スロー中も入り／抜けの長さを一定に保つ
	CutSlowElapsed += FApp::GetDeltaTime();

	const float Tin = FMath::Max( Params->GodSlashCutSlowEaseInTime, 0.0f );
	const float Thold = FMath::Max( Params->GodSlashCutSlowDuration, 0.0f );
	const float Tout = FMath::Max( Params->GodSlashCutSlowEaseOutTime, 0.0f );

	if ( CutSlowElapsed >= Tin + Thold + Tout )
	{
		EndCutSlow( true );	// 自然終了：プレイヤーも基準（神技スロー補正）へ戻して締める
		return;
	}

	// スロー強度 0..1（0＝基準速度、1＝最も遅い）。In で上昇 → Hold で 1 → Out で下降
	float Strength;
	if ( CutSlowElapsed < Tin )
	{
		Strength = ( Tin > 0.0f ) ? FMath::SmoothStep( 0.0f, 1.0f, CutSlowElapsed / Tin ) : 1.0f;
	}
	else if ( CutSlowElapsed < Tin + Thold )
	{
		Strength = 1.0f;
	}
	else
	{
		const float A = ( Tout > 0.0f ) ? ( CutSlowElapsed - Tin - Thold ) / Tout : 1.0f;
		Strength = 1.0f - FMath::SmoothStep( 0.0f, 1.0f, A );
	}

	// 基準速度 → 最も遅い倍率（Slow）を Strength で補間。プレイヤーと敵に同じ Strength を当てる
	const float Slow = FMath::Clamp( Params->GodSlashCutSlowDilation, 0.0f, 1.0f );

	OwnerCharacter->SetCustomTimeDilation( FMath::Lerp( GetSlashPlayerDilation(), Slow, Strength ) );
	if ( AActor* Enemy = CutSlowEnemy.Get() )
	{
		Enemy->CustomTimeDilation = FMath::Lerp( CutSlowEnemyBaseDilation, Slow, Strength );
	}
}

void UGodSlashSubModule::EndCutSlow( bool bRestorePlayerDilation )
{
	if ( !bCutSlowActive ) return;
	bCutSlowActive = false;

	if ( AActor* Enemy = CutSlowEnemy.Get() )
	{
		Enemy->CustomTimeDilation = CutSlowEnemyBaseDilation;
	}
	CutSlowEnemy = nullptr;
	CutSlowElapsed = 0.0f;

	// 自然終了時のみ神技スローの基準補正へ戻す。強制解除時は RestoreSlashSlowMotion が担うので二重設定しない
	if ( bRestorePlayerDilation && OwnerCharacter )
	{
		OwnerCharacter->SetCustomTimeDilation( GetSlashPlayerDilation() );
	}
}

void UGodSlashSubModule::ApplyFinishDamageToAllTargets()
{
	if ( !OwnerCharacter ) return;

	if ( const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData )
	{
		if ( Params->GodSlashFinishCameraShake )
		{
			if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
			{
				if ( APlayerCameraManager* CamMgr = PC->PlayerCameraManager )
				{
					CamMgr->StartCameraShake( Params->GodSlashFinishCameraShake, Params->GodSlashFinishCameraShakeScale );
				}
			}
		}
	}

	const FPlayerAttackParameterRow* AttackRow = FindGodSlashRow( OwnerCharacter, ActivatedGearIndex );

	UNiagaraSystem* LargeHitFx = OwnerCharacter->NiagaraSystemDataAsset
		? OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::HIT_LARGE )
		: nullptr;

	// 部位光輪は HP に関係なく一撃破壊する。連動（combo）成立を正しく判定させるため、アクターごとにまとめて
	// 一括破壊する（1つずつ壊すと連動成立前に個別モンタージュが流れてしまう）
	TMap<AActor*, TArray<USceneComponent*>> PartTargetsByActor;
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		ULockOnTargetComponent* Comp = Weak.Get();
		if ( !Comp ) continue;
		if ( AActor* TA = Comp->GetOwner() )
		{
			PartTargetsByActor.FindOrAdd( TA ).Add( Comp );
		}
	}
	for ( TPair<AActor*, TArray<USceneComponent*>>& Pair : PartTargetsByActor )
	{
		if ( UPartDestructionComponent* PartComp = Pair.Key->FindComponentByClass<UPartDestructionComponent>() )
		{
			PartComp->DestroyPartsByComponents( Pair.Value );
		}
	}

	// 部位単位でロックしているため、HP ダメージは同一アクターへ重複適用しない
	TSet<AActor*> Damaged;

	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		ULockOnTargetComponent* Comp = Weak.Get();
		if ( !Comp ) continue;

		// ダメージは同一アクター重複を除外するが、見た目はロックした部位それぞれに出す
		if ( LargeHitFx )
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				OwnerCharacter->GetWorld(), LargeHitFx, Comp->GetTargetLocation(), FRotator::ZeroRotator, FVector( 1.0f ) );
		}

		AActor* TargetActor = Comp->GetOwner();
		if ( !TargetActor || Damaged.Contains( TargetActor ) ) continue;

		IDamageable* Damageable = Cast<IDamageable>( TargetActor );
		if ( !Damageable ) continue;

		FDamageInfo Info;
		Info.Instigator = OwnerCharacter;
		Info.AttackTypeTag = TAG_AttackType_Player_GodActionSlash;

		if ( AttackRow )
		{
			Info.BaseDamage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );
			Info.StaggerDamage = AttackRow->StaggerDamage;

			const ACharacter* TargetChar = Cast<ACharacter>( TargetActor );
			const bool bTargetAirborne = TargetChar
				&& TargetChar->GetCharacterMovement()
				&& TargetChar->GetCharacterMovement()->IsFalling();
			Info.HitReactionTag = ( bTargetAirborne && AttackRow->EnemyAirReactionTag.IsValid() )
				? AttackRow->EnemyAirReactionTag
				: AttackRow->EnemyReactionTag;

			Info.bUseHitStop = AttackRow->bUseHitStop;
			Info.HitStopDuration = AttackRow->HitStopDuration;
			Info.HitStopDilation = AttackRow->HitStopDilation;
		}
		else
		{
			// テーブル未設定時でも吹き飛びだけは保証する
			Info.BaseDamage = OwnerCharacter->CalculateDamage( 1.0f );
			Info.HitReactionTag = TAG_HitReaction_Blowoff_M;
		}

		FHitResult Hit;
		Hit.Location = Comp->GetTargetLocation();
		Hit.ImpactPoint = Comp->GetTargetLocation();
		Info.HitResult = Hit;

		Damageable->ReceiveDamage( Info );
		Damaged.Add( TargetActor );
	}
}

void UGodSlashSubModule::BeginSlashSlowMotion()
{
	if ( bSlowMoActive ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	// ワールド全体をスローにし、プレイヤーだけ等速側に補正する（DodgeActionPlayerModule と同流儀）。
	// 構えが終わって斬りかかりへ移る瞬間に呼ぶ（構え中は通常速度で見せる）
	const float GlobalDilation = FMath::Clamp( Params->GodSlashGlobalTimeDilation, 0.01f, 1.0f );
	UGameplayStatics::SetGlobalTimeDilation( World, GlobalDilation );
	OwnerCharacter->SetCustomTimeDilation( Params->GodSlashPlayerTimeScale / GlobalDilation );

	// 鳥 mover もプレイヤー扱いにする。補正しないと鳥だけワールドスローに引きずられ、
	// トレイル Niagara も待機モーションも激遅になって軌跡が出ない
	if ( !IsMoverCharacter() )
	{
		if ( AActor* Mover = GetMover() )
		{
			Mover->CustomTimeDilation = GetSlashPlayerDilation();
		}
	}

	bSlowMoActive = true;
}

void UGodSlashSubModule::StartSlashStaging()
{
	if ( bStagingActive ) return;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// スローはここでは掛けず、構えを通常速度で見せてから BeginSlashSlowMotion で開始する
	if ( OwnerCharacter->GodSlashCameraParam )
	{
		if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					SlashCameraHandle = CameraSubsystem->PushCameraMode( OwnerCharacter->GodSlashCameraParam );
				}
			}
		}
	}

	bStagingActive = true;
}

void UGodSlashSubModule::RestoreSlashSlowMotion()
{
	if ( !bSlowMoActive ) return;
	bSlowMoActive = false;

	if ( !OwnerCharacter ) return;

	if ( UWorld* World = OwnerCharacter->GetWorld() )
	{
		UGameplayStatics::SetGlobalTimeDilation( World, 1.0f );
	}
	// スロー中（高速 CustomTimeDilation）に入ったヒットストップは復帰先にその高速値を記録しているため、
	// 先に解除してから 1.0 を設定する（Finisher / Dodge と同流儀）
	HitStopUtil::CancelHitStop( OwnerCharacter );
	OwnerCharacter->SetCustomTimeDilation( 1.0f );

	// 補正で速めていた鳥 mover も戻す（残すと逆に激速になる）
	if ( !IsMoverCharacter() )
	{
		if ( AActor* Mover = GetMover() )
		{
			Mover->CustomTimeDilation = 1.0f;
		}
	}
}

void UGodSlashSubModule::EndSlashStaging()
{
	if ( !bStagingActive ) return;
	bStagingActive = false;

	// スローがまだ掛かっていれば戻す（ED で先に解除済みのこともある）
	RestoreSlashSlowMotion();

	if ( OwnerCharacter && SlashCameraHandle.IsValid() )
	{
		if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					// Pop 前に PC の ControlRotation を ED カメラの水平角へ合わせる（ThirdPersonCamera は
					// ControlRotation 基準で位置を計算するため、合わせないと Pop 直後に別方向へ回転して戻る）
					if ( UExCameraModeComponent* ModeComp = CameraSubsystem->GetCameraModeComponent() )
					{
						const FMinimalViewInfo& LastView = ModeComp->GetLastFrameViewInfo();
						FVector CamToPlayer = OwnerCharacter->GetActorLocation() - LastView.Location;
						CamToPlayer.Z = 0.0f;
						if ( !CamToPlayer.IsNearlyZero() )
						{
							FRotator NewRot = PC->GetControlRotation();
							NewRot.Yaw = CamToPlayer.GetSafeNormal().Rotation().Yaw;
							PC->SetControlRotation( NewRot );
						}
					}

					const FName PopBlend = ( OwnerCharacter->PlayerParamData )
						? OwnerCharacter->PlayerParamData->GodSlashCamPopBlendRowName
						: NAME_None;
					CameraSubsystem->PopCameraMode( SlashCameraHandle, PopBlend );
				}
			}
		}
	}

	SlashCameraHandle.Clear();
}

bool UGodSlashSubModule::GetCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const
{
	if ( CurrentState != EGodSlashState::Slashing ) return false;

	// 以下の Focus/Target/CutIndex はクラシック演出専用でワイドカット中は使わない
	bOutWideCut = bWideCutActive;
	bOutWideCutFinishing = ( SlashPhase == ESlashPhase::Return || SlashPhase == ESlashPhase::End );

	// ED 突入時は CurrentSlashIndex がロック数まで進んでおり、そのまま渡すとアングル固定が外れて選び直され、
	// GetTargetAt も範囲外で null になって「別カメラ」になる。最後の有効カットに固定して渡す
	const int32 EffectiveCutIndex = ( LockedTargets.Num() > 0 )
		? FMath::Clamp( CurrentSlashIndex, 0, LockedTargets.Num() - 1 )
		: CurrentSlashIndex;

	OutFocusLocation = CameraFocusLocation;
	OutCutIndex = EffectiveCutIndex;
	// カメラはこれを合図に寄った構図でポーズを固定する（ED 中も true のまま＝固定ポーズを保持し続ける）
	bOutCutLanded = bCutHitFired;

	// 巨大敵対応：対象部位が属する敵の「見た目の体」サイズを構図へ渡す
	OutTargetCenter = CameraFocusLocation;
	OutTargetRadius = GetTargetMeshRadius( GetTargetAt( EffectiveCutIndex ), OutTargetCenter );
	return true;
}

// --- ギア段階に応じた性能（一閃）---

int32 UGodSlashSubModule::GetMaxLockOnCountForGear() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return 1;

	const TArray<int32>& Counts = OwnerCharacter->PlayerParamData->GodSlashLockOnCountForGear;
	if ( Counts.Num() == 0 ) return 1;

	// この時点ではチャージがリセット済みのため、ライブのギア値ではなく開始時に確定した段階を使う
	const int32 GearIdx = FMath::Clamp( ActivatedGearIndex, 0, Counts.Num() - 1 );
	return FMath::Max( 1, Counts[GearIdx] );
}

float UGodSlashSubModule::GetLockOnDistanceForGear() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return 3000.0f;

	const TArray<float>& Dists = OwnerCharacter->PlayerParamData->GodSlashLockOnDistanceForGear;
	if ( Dists.Num() == 0 ) return 3000.0f;

	const int32 GearIdx = FMath::Clamp( ActivatedGearIndex, 0, Dists.Num() - 1 );
	return Dists[GearIdx];
}

float UGodSlashSubModule::GetWideCutZigzagMoveSpeed() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return 20000.0f;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	// 1〜2 体は対象中心の小さな周回（五芒星）になり 1 区間の移動距離の傾向が変わる。
	// 1 体と 2 体も対象間移動の有無で体感速度が違うため、それぞれ別倍率を用意している
	if ( LockedTargets.Num() == 1 )
	{
		const float SingleTargetScale = FMath::Max( 0.0f, Params->GodSlashWideCutSingleTargetSpeedScale );
		return Params->GodSlashWideCutZigzagMoveSpeed * SingleTargetScale;
	}
	if ( LockedTargets.Num() == 2 )
	{
		const float DualTargetScale = FMath::Max( 0.0f, Params->GodSlashWideCutDualTargetSpeedScale );
		return Params->GodSlashWideCutZigzagMoveSpeed * DualTargetScale;
	}

	// 対象数が多いほど星型 1 周の総距離が伸びるため、1 体につき ...SpeedPerTarget ぶん基準速度へ上乗せする
	const int32 ExtraTargets = FMath::Max( 0, LockedTargets.Num() - 1 );
	return Params->GodSlashWideCutZigzagMoveSpeed + Params->GodSlashWideCutZigzagSpeedPerTarget * (float)ExtraTargets;
}

ULockOnTargetComponent* UGodSlashSubModule::GetTargetAt( int32 Index ) const
{
	if ( !LockedTargets.IsValidIndex( Index ) ) return nullptr;
	return LockedTargets[Index].Get();
}

float UGodSlashSubModule::GetTargetMeshRadius( const ULockOnTargetComponent* Comp, FVector& OutCenter ) const
{
	if ( !Comp ) return 0.0f;

	// AActor::GetActorBounds は索敵用の巨大なスフィアコリジョンまで含みカメラが遠くへ飛ぶため使わない。
	// スケルタルメッシュの原点と水平半幅を使う（高さは足↔頭で極端になりやすい）
	if ( const ACharacter* EnemyChar = Cast<ACharacter>( Comp->GetOwner() ) )
	{
		if ( const USkeletalMeshComponent* MeshComp = EnemyChar->GetMesh() )
		{
			const FBoxSphereBounds MeshBounds = MeshComp->Bounds;
			OutCenter = MeshBounds.Origin;
			return FMath::Max( MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y );
		}
	}
	return 0.0f;
}

void UGodSlashSubModule::GetLockedTargetsGroupBounds( FVector& OutCenter, float& OutRadius ) const
{
	OutCenter = OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;
	OutRadius = 0.0f;

	TArray<FVector> Centers;
	TArray<float> Radii;
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		ULockOnTargetComponent* Comp = Weak.Get();
		if ( !Comp ) continue;

		FVector Center;
		const float Radius = GetTargetMeshRadius( Comp, Center );
		if ( Radius <= 0.0f )
		{
			Center = Comp->GetTargetLocation();	// メッシュが取れなくても位置だけは重心計算に含める
		}
		Centers.Add( Center );
		Radii.Add( Radius );
	}
	if ( Centers.Num() == 0 ) return;

	FVector Sum = FVector::ZeroVector;
	for ( const FVector& C : Centers ) Sum += C;
	OutCenter = Sum / (float)Centers.Num();

	float MaxReach = 0.0f;
	for ( int32 i = 0; i < Centers.Num(); ++i )
	{
		MaxReach = FMath::Max( MaxReach, FVector::Dist( OutCenter, Centers[i] ) + Radii[i] );
	}
	OutRadius = MaxReach;
}

void UGodSlashSubModule::BuildWideCutStarOrder()
{
	WideCutStarOrder.Reset();
	WideCutCentroid = OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;

	const int32 N = LockedTargets.Num();
	if ( N == 0 ) return;

	if ( N <= 2 )
	{
		// 2 体以下は星型を作れないため単純往復（0 / 0,1 を繰り返す）にする
		for ( int32 i = 0; i < N; ++i ) WideCutStarOrder.Add( i );
	}

	// 重心を基準に各対象を角度でソートし、円周上に並んだ順番を作る
	FVector Centroid = FVector::ZeroVector;
	TArray<FVector> Locations;
	Locations.Reserve( N );
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		const ULockOnTargetComponent* Comp = Weak.Get();
		const FVector Loc = Comp ? Comp->GetTargetLocation() : FVector::ZeroVector;
		Locations.Add( Loc );
		Centroid += Loc;
	}
	Centroid /= (float)N;
	WideCutCentroid = Centroid;	// GetWideCutStarWaypoint の基準点

	if ( N <= 2 ) return;

	TArray<int32> AngularOrder;
	AngularOrder.Reserve( N );
	for ( int32 i = 0; i < N; ++i ) AngularOrder.Add( i );
	AngularOrder.Sort( [&]( int32 A, int32 B )
	{
		const float AngleA = FMath::Atan2( Locations[A].Y - Centroid.Y, Locations[A].X - Centroid.X );
		const float AngleB = FMath::Atan2( Locations[B].Y - Centroid.Y, Locations[B].X - Centroid.X );
		return AngleA < AngleB;
	} );

	// 星型多角形のステップ幅：N と互いに素になるまで N/2 から減らす（例：N=5→2＝五芒星、N=8→3）
	int32 Step = FMath::Max( 1, N / 2 );
	while ( Step > 1 && GreatestCommonDivisorInt( Step, N ) != 1 )
	{
		--Step;
	}

	// Step 飛ばしで辿ることで、隣同士ではなく対角側へ跳ぶ巡回順になる
	WideCutStarOrder.Reserve( N );
	int32 Cur = 0;
	for ( int32 i = 0; i < N; ++i )
	{
		WideCutStarOrder.Add( AngularOrder[Cur] );
		Cur = ( Cur + Step ) % N;
	}
}

int32 UGodSlashSubModule::GetWideCutTargetIndexForStep( int32 Step ) const
{
	if ( WideCutStarOrder.Num() == 0 ) return 0;
	return WideCutStarOrder[ Step % WideCutStarOrder.Num() ];
}

FVector UGodSlashSubModule::GetWideCutStarWaypoint( const FVector& RawTargetLoc, int32 Step ) const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return RawTargetLoc;

	const UTidePlayerParamDataAsset* Params = OwnerCharacter->PlayerParamData;

	FVector BasePoint;
	FVector HorizOffset;

	if ( LockedTargets.Num() <= 2 )
	{
		// 1 体だと重心＝対象位置で水平移動量が 0（その場で上下にバウンドするだけ）、2 体だと毎周回まったく同じ
		// 2 点を行き来して見える。どちらも「対象を中心に小さな五芒星を描いて舞う」動きを重ねて解決する
		constexpr int32 VirtualPointCount = 5;
		constexpr int32 VirtualStepSkip = 2;	// 5 点を 2 つ飛ばしで辿ると五芒星になる
		const int32 VirtualIndex = ( Step * VirtualStepSkip ) % VirtualPointCount;
		const float AngleRad = ( 2.0f * PI * (float)VirtualIndex ) / (float)VirtualPointCount;
		const float OrbitRadius = FMath::Max( 0.0f, Params->GodSlashWideCutSingleTargetOrbitRadius );

		BasePoint = RawTargetLoc;
		HorizOffset = FVector( FMath::Cos( AngleRad ), FMath::Sin( AngleRad ), 0.0f ) * OrbitRadius;
	}
	else
	{
		// 水平方向を対象の実位置に揃えて明らかな空振りに見えないようにし、倍率で星型の大きさだけ調整する
		FVector Horiz = RawTargetLoc - WideCutCentroid;
		Horiz.Z = 0.0f;
		const float HorizDist = Horiz.Size();
		Horiz = Horiz.GetSafeNormal();
		if ( Horiz.IsNearlyZero() )
		{
			Horiz = FVector::ForwardVector;
		}

		const float RadiusScale = FMath::Max( 0.0f, Params->GodSlashWideCutStarRadiusScale );

		BasePoint = WideCutCentroid;
		HorizOffset = Horiz * ( HorizDist * RadiusScale );
	}

	// 高さは Step の偶奇で上下交互に振る。下方向は地面へ潜ると Z が押し戻され続けて到達判定が満たせなく
	// なるため、真下へトレースして地面までの余裕を超えない範囲だけ許可する
	const float HeightVariation = FMath::Max( 0.0f, Params->GodSlashWideCutStarHeightVariation );
	const float ZigZagSign = ( Step % 2 == 0 ) ? 1.0f : -1.0f;
	float HeightOffset = ZigZagSign * HeightVariation;

	if ( HeightOffset < 0.0f )
	{
		if ( UWorld* World = OwnerCharacter->GetWorld() )
		{
			const float Clearance = FMath::Max( 0.0f, Params->GodSlashGroundClearance );
			const FVector TraceStart = RawTargetLoc;
			const FVector TraceEnd = RawTargetLoc - FVector( 0.0f, 0.0f, HeightVariation + Clearance + 50.0f );

			FHitResult Hit;
			FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodSlashWideCutStarGroundCheck ), false, OwnerCharacter );
			const FCollisionObjectQueryParams ObjectParams = MakeGroundObjectQuery();

			if ( World->LineTraceSingleByObjectType( Hit, TraceStart, TraceEnd, ObjectParams, QueryParams ) )
			{
				const float MaxDown = FMath::Max( 0.0f, ( RawTargetLoc.Z - Hit.Location.Z ) - Clearance );
				HeightOffset = -FMath::Min( HeightVariation, MaxDown );
			}
		}
	}

	FVector Waypoint = BasePoint + HorizOffset;
	Waypoint.Z = RawTargetLoc.Z + HeightOffset;
	return Waypoint;
}

void UGodSlashSubModule::GetLockedTargetLocations( TArray<FVector>& OutLocations ) const
{
	OutLocations.Reset();
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		if ( ULockOnTargetComponent* Comp = Weak.Get() )
		{
			OutLocations.Add( Comp->GetTargetLocation() );
		}
	}
}

void UGodSlashSubModule::GetLockedTargetActors( TArray<AActor*>& OutActors ) const
{
	OutActors.Reset();
	for ( const TWeakObjectPtr<ULockOnTargetComponent>& Weak : LockedTargets )
	{
		if ( ULockOnTargetComponent* Comp = Weak.Get() )
		{
			if ( AActor* Owner = Comp->GetOwner() )
			{
				OutActors.AddUnique( Owner );	// 同一敵の複数部位ロックを 1 体に集約
			}
		}
	}
}

// --- デバッグ ---

#if !UE_BUILD_SHIPPING
void UGodSlashSubModule::DrawDebugImGui()
{
	ImGui::Text( "一閃ステート: " );
	ImGui::SameLine();
	switch ( CurrentState )
	{
	case EGodSlashState::Idle:		ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "Idle" ); break;
	case EGodSlashState::LockingOn:	ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ), "LockingOn" ); break;
	case EGodSlashState::Slashing:	ImGui::TextColored( ImVec4( 0.2f, 1.0f, 0.4f, 1.0f ), "Slashing" ); break;
	}

	ImGui::Text( "発動時ギア: %d", ActivatedGearIndex );
	ImGui::Text( "ロックオン数: %d / %d", LockedTargets.Num(), GetMaxLockOnCountForGear() );
	ImGui::Text( "ロックオン距離: %.0f", GetLockOnDistanceForGear() );
	ImGui::Text( "演出モード: %s", bWideCutActive ? "ワイドカット" : "クラシック" );

	if ( CurrentState == EGodSlashState::Slashing )
	{
		const char* PhaseStr = "Stance";
		if ( SlashPhase == ESlashPhase::Loop ) PhaseStr = "Loop";
		else if ( SlashPhase == ESlashPhase::Return ) PhaseStr = "Return";
		else if ( SlashPhase == ESlashPhase::End ) PhaseStr = "End";

		if ( bWideCutActive && SlashPhase == ESlashPhase::Loop )
		{
			// 何周も繰り返すため、対象消化数ではなく周回カウンタ・経過時間を表示する
			ImGui::Text( "一閃フェーズ: %s (周回 %d, 対象 %d/%d)", PhaseStr, WideCutStepCounter, CurrentSlashIndex + 1, LockedTargets.Num() );
			ImGui::Text( "ワイドカット周回時間: %.2f / %.2f", WideCutLoopElapsed,
				OwnerCharacter && OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->GodSlashWideCutLoopDuration : 0.0f );
		}
		else
		{
			ImGui::Text( "一閃フェーズ: %s (%d/%d)", PhaseStr, CurrentSlashIndex + 1, LockedTargets.Num() );
		}
	}
}
#endif
