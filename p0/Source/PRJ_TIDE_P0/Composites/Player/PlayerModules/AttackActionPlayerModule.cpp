// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AttackActionPlayerModule.h"

#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Utilities/TargetingUtils.h"
#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotifyState_CommonAttack.h"

#include "Animation/AnimMontage.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

namespace
{
	// モンタージュ内で最初に発生する CommonAttack 判定枠の開始秒（無ければ負値）。
	// ChargeActionPlayerModule_V2::GetFirstHitWindowTriggerTime と同じ走査
	float FindFirstHitWindowTriggerTime( const UAnimMontage* Montage )
	{
		if ( !Montage ) return -1.0f;

		float Best = -1.0f;
		for ( const FAnimNotifyEvent& Event : Montage->Notifies )
		{
			if ( !Cast<UAnimNotifyState_CommonAttack>( Event.NotifyStateClass ) ) continue;

			const float TriggerTime = Event.GetTriggerTime();
			if ( Best < 0.0f || TriggerTime < Best ) Best = TriggerTime;
		}
		return Best;
	}
}

void UAttackActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UAttackActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// R2 を離していれば振り下ろしへ割り込める。ST の間は IsPlayingChargeAction() が true のままなので、
	// ここで除外しないと ST に仕込んだ CanAttack が受付に届かない
	const bool bChargeJumpDiveWindow = OwnerCharacter->IsChargeJumpNormalAttackWindow();

	// チャージの優先期間中は通常攻撃をブロックする
	const bool bIsChargePriority = !bChargeJumpDiveWindow &&
		( OwnerCharacter->IsCharging() ||
		OwnerCharacter->IsPlayingChargeAction() ||
		( OwnerCharacter->GetCurrentChargeComboIndex() > 1 ) );

	// 現在のステートに応じて、入力を受け付けるタグを厳密に分ける
	bool bIsAttackTiming = false;
	if ( CurrentState == EAttackState::Attacking )
	{
		// 発動直後は前段モンタージュのタグが残留しているため受付自体を行わない。
		// 入力はバッファに残るので、窓が明けてタグが正しくなった時点で発火する
		if ( IsWithinLightAttackTagResidueWindow() )
		{
			bIsAttackTiming = false;
		}
		else if ( CurrentLightComboIndex >= GetMaxLightComboCount() )
		{
			// 最終段からのループ（最終段→1）は厳密に CanAttack のみを要求する
			bIsAttackTiming = OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack );
		}
		else
		{
			bIsAttackTiming = OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
		}
	}
	else
	{
		bIsAttackTiming = OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack );
	}

	// 滑空ループは CanAttack を持たず、空中チャージ使い切り後は残ったコンボ段が優先ブロックに引っかかる。
	// どちらもタグ／優先ブロックを迂回して TryStartAirNormalDiveAttack へ直接流し、「攻撃で振り下ろしへ」を保証する
	const bool bAirDiveInterruptWindow =
		( OwnerCharacter->IsInGlideSession() || OwnerCharacter->IsAirActionLimitedAfterAirCharge() )
		&& !OwnerCharacter->IsPlayingChargeAction();
	if ( bAirDiveInterruptWindow )
	{
		const float BufferTime = OwnerCharacter->PlayerParamData->AttackBufferTime;
		if ( TryConsumeCommand( TAG_Input_Command_Attack, BufferTime ) )
		{
			OwnerCharacter->TryStartAirNormalDiveAttack();
		}
	}
	else if ( !bIsChargePriority && bIsAttackTiming )
	{
		const float BufferTime = OwnerCharacter->PlayerParamData->AttackBufferTime;
		if ( TryConsumeCommand( TAG_Input_Command_Attack, BufferTime ) )
		{
			// 空中の非チャージ攻撃は振り下ろし（縦ダイブ）へ。
			// ChargeModule が発動を引き受けたら地上ライトコンボは出さない
			if ( !OwnerCharacter->TryStartAirNormalDiveAttack() )
			{
				RequestAttack( EPlayerAttackType::Light );
			}
		}
	}

	if ( CurrentState == EAttackState::Idle ) return;

	// 攻撃中に回避ステートへ移行したら、移動制限のロックを即座に手放す
	if ( OwnerCharacter && OwnerCharacter->IsDodging() )
	{
		CancelAttack();
		return;
	}

	if ( CurrentState == EAttackState::Attacking )
	{
		// ヒットバック（自己ノックバック）中はルートモーションをバイパスしてカプセルを直接動かす
		if ( !HitBackTimer.IsFinish() )
		{
			HitBackTimer.Update( DeltaTime );

			OwnerCharacter->SetAnimRootMotionTranslationScale( 0.0f );

			const float HitBackDeceleration = OwnerCharacter->PlayerParamData->LightAttackHitBackDeceleration;
			CurrentHitBackVelocity = FMath::VInterpConstantTo( CurrentHitBackVelocity, FVector::ZeroVector, DeltaTime, HitBackDeceleration );

			OwnerCharacter->AddActorWorldOffset( CurrentHitBackVelocity * DeltaTime, true );	// bSweep で壁抜け防止
		}
		else
		{
			// めり込み防止
			if ( IsFrontBlockedByPawn() )
			{
				OwnerCharacter->SetAnimRootMotionTranslationScale( 0.0f );
			}
			else
			{
				// 判定が出るまでの区間だけ詰め倍率で踏み込む。以降を素に戻さないと
				// 振り抜き・引き足まで同じ倍率で伸びて対象を追い越す
				const bool bInApproachWindow = AttackTimer.GetElapsed() < LightAttackApproachEndTime;
				OwnerCharacter->SetAnimRootMotionTranslationScale( bInApproachWindow ? LightAttackApproachScale : 1.0f );
			}
		}
	}

	AttackTimer.Update( DeltaTime );

	if ( AttackTimer.IsFinish() )
	{
		OnEndAttack();
	}
}

bool UAttackActionPlayerModule::RequestAttack( EPlayerAttackType InType, bool bForce )
{
	if ( !OwnerCharacter ) return false;
	if ( OwnerCharacter->IsHitReacting() ) return false;

	// 地上の弱攻撃は入力由来・チャージコンボ由来を問わず一律弾く（bForce も対象）。
	// 空中の振り下ろしは Charged 種別なので影響しない
	if ( InType == EPlayerAttackType::Light && OwnerCharacter->IsGroundNormalAttackMasked() ) return false;

	// bForce（滑空キャンセル等）はタイミングタグ判定を丸ごと迂回する（可否は呼び出し側で判定済み）
	if ( !bForce && CurrentState == EAttackState::Attacking )
	{
		// 連打による 1 段目ループ（出だしキャンセル）を防ぐ
		if ( InType == EPlayerAttackType::Light )
		{
			// 発動直後は前段の CanCombo / CanAttack が残留していて、始まったばかりの段を
			// 次段が即座に上書きしてしまう（モーション飛び）
			if ( IsWithinLightAttackTagResidueWindow() ) return false;

			if ( CurrentLightComboIndex >= GetMaxLightComboCount() )
			{
				// 最終段からは CanAttack がある場合のみ 1 段目へのリクエストを許可
				if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )
				{
					return false;
				}
			}
			else
			{
				if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) ) return false;
			}
		}
	}
	else if ( !bForce )
	{
		if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )
		{
			return false;
		}
	}

	// 攻撃中からの継続だけが「コンボ成功」で、それ以外は必ず 1 段目から始める
	// （Idle からの発動で CanCombo を見ると、直前の残留タグで 1 段目を飛ばして 2→3→2 に見える）
	if ( InType == EPlayerAttackType::Light )
	{
		const bool bContinueCombo =
			CurrentState == EAttackState::Attacking &&
			CurrentLightComboIndex < GetMaxLightComboCount() &&
			OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );

		CurrentLightComboIndex = bContinueCombo ? CurrentLightComboIndex + 1 : 1;
	}

	OnStartAttack( InType );
	return true;
}

void UAttackActionPlayerModule::CancelAttack()
{
	if ( CurrentState == EAttackState::Idle ) return;

	if ( OwnerCharacter )
	{
		UAnimMontage* AttackMontage = nullptr;
		switch ( CurrentAttackType )
		{
		case EPlayerAttackType::Light:
			AttackMontage = GetAnimMontage( GetLightAttackAnimTag() );
			break;
		case EPlayerAttackType::Charged:
		{
			FName CurrentAttackTag = OwnerCharacter->GetChargeAttackAnimTag();
			AttackMontage = GetAnimMontage( CurrentAttackTag );
		} break;
		default:	break;
		}
		if ( AttackMontage && OwnerCharacter->GetCurrentMontage() == AttackMontage )
		{
			OwnerCharacter->StopAnimMontage( AttackMontage );
		}
		OwnerCharacter->RefreshMovementParams();
		OwnerCharacter->SetAnimRootMotionTranslationScale( 1.0f );
	}

	HitBackTimer.Clear();
	CurrentHitBackVelocity = FVector::ZeroVector;
	bHitBackIsBreakthrough = false;

	CurrentState = EAttackState::Idle;
	CurrentAttackType = EPlayerAttackType::None;
	AttackTimer.Clear();
	bWasDashingBeforeAttack = false;
	CurrentLightComboIndex = 1;
	LightAttackApproachScale = 1.0f;
	LightAttackApproachEndTime = 0.0f;
}

void UAttackActionPlayerModule::OnStartAttack( EPlayerAttackType InType )
{
	if ( !OwnerCharacter ) return;
	bWasDashingBeforeAttack = OwnerCharacter->IsDashing();
	OwnerCharacter->CancelDodge();

	CurrentState = EAttackState::Attacking;
	CurrentAttackType = InType;
	LightAttackApproachScale = 1.0f;
	LightAttackApproachEndTime = 0.0f;

	if( CurrentAttackType != EPlayerAttackType::Charged )
	{
		OwnerCharacter->StopDash();
	}

	// ターゲット吸着（振り向き）。スティック入力があればその方向、なければキャラの正面を基準にする
	if ( InType == EPlayerAttackType::Light )
	{
		FVector BaseDir = FVector::ZeroVector;
		const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();

		if ( !RawInput.IsNearlyZero() )
		{
			BaseDir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
		}
		else
		{
			BaseDir = OwnerCharacter->GetActorForwardVector();
			BaseDir.Z = 0.0f;
			BaseDir.Normalize();
		}

		ULockOnTargetComponent* FoundTarget = nullptr;
		const FVector HomingDir = GetHomingDirection( BaseDir, FoundTarget );

		// 2段目以降で、すでに敵に密着している場合は振り向くのを防ぐ
		const bool bIsTooCloseToTurn = OwnerCharacter->IsPushingPawn();
		constexpr int32 ComboStage2 = 2;

		if ( CurrentLightComboIndex >= ComboStage2 && bIsTooCloseToTurn )
		{
			// 密着状態では今の正面を維持する。カプセル側面に少しズレた際の 180 度反転を防ぐ
		}
		else
		{
			OwnerCharacter->SetActorRotation( HomingDir.Rotation() );
		}

		// 振り向き先と同じ対象へ、ルートモーションの前進量を伸ばして届かせる
		SetupLightAttackApproach( FoundTarget );
	}

	const float Duration = PlayAttackMontage( InType );
	if ( Duration > 0.0f )
	{
		AttackTimer.Set( Duration );
	}
	else
	{
		OnEndAttack();
	}
}

void UAttackActionPlayerModule::OnEndAttack()
{
	if ( !OwnerCharacter ) return;

	if ( CurrentAttackType != EPlayerAttackType::Charged )
	{
		OwnerCharacter->RefreshMovementParams();
	}

	OwnerCharacter->SetAnimRootMotionTranslationScale( 1.0f );
	bWasDashingBeforeAttack = false;

	HitBackTimer.Clear();
	CurrentHitBackVelocity = FVector::ZeroVector;
	bHitBackIsBreakthrough = false;

	CurrentState = EAttackState::Idle;
	CurrentAttackType = EPlayerAttackType::None;
	AttackTimer.Clear();
	CurrentLightComboIndex = 1;
	CurrentHomingTarget = nullptr;
	LightAttackApproachScale = 1.0f;
	LightAttackApproachEndTime = 0.0f;
}

void UAttackActionPlayerModule::OnAttackHit( AActor* TargetActor, bool bIsRebounded )
{
	if ( !TargetActor || !OwnerCharacter ) return;

	// ターゲットから自分への方向（＝後ろに下がる方向）へ初速を張る
	FVector PushbackDir = ( OwnerCharacter->GetActorLocation() - TargetActor->GetActorLocation() ).GetSafeNormal2D();

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float PushbackPower = PlayerParams ? PlayerParams->LightAttackSelfPushbackPower : 1000.0f;
	const float HitBackDuration = PlayerParams ? PlayerParams->LightAttackHitBackDuration : 0.2f;

	CurrentHitBackVelocity = PushbackDir * PushbackPower;
	HitBackTimer.Set( HitBackDuration );
	bHitBackIsBreakthrough = false;
}

void UAttackActionPlayerModule::CancelHitBack()
{
	HitBackTimer.Clear();
	CurrentHitBackVelocity = FVector::ZeroVector;
	bHitBackIsBreakthrough = false;
}

void UAttackActionPlayerModule::ApplyBreakthroughMove( const FVector& Direction, float Speed )
{
	// 同フレームの OnAttackHit でヒットバックを張ったモジュールだけが前進を引き受ける
	if ( HitBackTimer.IsFinish() ) return;
	// 同フレームに 2 回解決されても前進は 1 回だけ
	if ( bHitBackIsBreakthrough ) return;

	if ( !OwnerCharacter || Speed <= 0.0f || Direction.IsNearlyZero() )
	{
		CancelHitBack();	// 前進量が無いなら後退だけ消す（その場停止）
		return;
	}

	// ヒットバックと同じ強制移動の枠を使い、向きだけ前方へ差し替える。持続・減速はそのまま流用する
	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float Duration = PlayerParams ? PlayerParams->LightAttackHitBackDuration : 0.2f;

	CurrentHitBackVelocity = Direction.GetSafeNormal2D() * Speed;
	HitBackTimer.Set( Duration );
	bHitBackIsBreakthrough = true;
}

float UAttackActionPlayerModule::PlayAttackMontage( EPlayerAttackType InType )
{
	if ( !OwnerCharacter ) return 0.0f;

	FName Tag = PlayerAnimTags::LIGHT_ATK_01;
	float PlayRate = 1.0f;

	switch ( InType )
	{
	case EPlayerAttackType::Light:
	{
		Tag = GetLightAttackAnimTag();
	} break;
	case EPlayerAttackType::Charged:
	{
		Tag = OwnerCharacter->GetChargeAttackAnimTag();

		const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;

		// ギア段階（1〜4）を配列インデックス（0〜3）へ
		const int32 GearIdx = FMath::Clamp( OwnerCharacter->GetCurrentChargeGearIndex() - 1, 0, PlayerParams->MaxChargeGearCount - 1 );
		if ( PlayerParams->ChargeAttackAnimPlayRatesForGear.IsValidIndex( GearIdx ) )
		{
			PlayRate = PlayerParams->ChargeAttackAnimPlayRatesForGear[GearIdx];
		}

		// 空中チャージ攻撃の ST（溜め）は専用の再生速度で上書きする
		if ( Tag == PlayerAnimTags::AIRCHARGE_ATK_ST )
		{
			PlayRate = PlayerParams->AirChargeAttackStartPlayRate;
		}
	} break;
	}

	return PlayAnimMontage( Tag, PlayRate );
}

int32 UAttackActionPlayerModule::GetMaxLightComboCount() const
{
	constexpr int32 DefaultMaxCount = 2;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return DefaultMaxCount;
	return FMath::Max( 1, OwnerCharacter->PlayerParamData->MaxLightComboCount );
}

bool UAttackActionPlayerModule::IsWithinLightAttackTagResidueWindow() const
{
	if ( CurrentState != EAttackState::Attacking ) return false;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;

	// AttackTimer は OnStartAttack でモンタージュ長を張るので、経過秒＝この攻撃が始まってからの時間
	const float IgnoreTime = OwnerCharacter->PlayerParamData->LightAttackTagResidueIgnoreTime;
	if ( IgnoreTime <= 0.0f ) return false;

	return !AttackTimer.IsFinish() && AttackTimer.GetElapsed() < IgnoreTime;
}

FName UAttackActionPlayerModule::GetLightAttackAnimTag() const
{
	switch ( CurrentLightComboIndex )
	{
	case 1: return PlayerAnimTags::LIGHT_ATK_01;
	case 2: return PlayerAnimTags::LIGHT_ATK_02;
	case 3: return PlayerAnimTags::LIGHT_ATK_03;
	case 4: return PlayerAnimTags::LIGHT_ATK_04;
	}
	return PlayerAnimTags::LIGHT_ATK_01;
}

FVector UAttackActionPlayerModule::GetHomingDirection( const FVector& InDefaultDir, ULockOnTargetComponent*& OutTargetComp ) const
{
	OutTargetComp = nullptr;
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData )
	{
		return InDefaultDir;
	}

	const float MaxDist = OwnerCharacter->PlayerParamData->LightAttackHomingDistance;
	const float HalfAngleRad = FMath::DegreesToRadians( OwnerCharacter->PlayerParamData->LightAttackHomingAngle * 0.5f );
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	ULockOnTargetComponent* LockedTargetComp = nullptr;
	if ( OwnerCharacter->IsLockOnActive() && OwnerCharacter->GetLockOnComponent() != nullptr )
	{
		LockedTargetComp = OwnerCharacter->GetLockOnComponent()->GetTarget();
	}

	FVector ResultDir = InDefaultDir;
	ULockOnTargetComponent* FinalTargetComp = nullptr;

	// ロックオン対象を優先し、無ければスティック入力方向を基準に扇形サーチ
	if ( LockedTargetComp != nullptr )
	{
		ResultDir = ( LockedTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		FinalTargetComp = LockedTargetComp;
	}
	else
	{
		FinalTargetComp = TargetingUtil::FindBestTargetInFan(
			OwnerCharacter->GetWorld(), MyLoc, InDefaultDir, MaxDist, HalfAngleRad, OwnerCharacter,
			OwnerCharacter->PlayerParamData->LightAttackHomingApexBackOffset,
			OwnerCharacter->PlayerParamData->LightAttackHomingMaxHeightDiff );

		if ( FinalTargetComp )
		{
			ResultDir = ( FinalTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		}
	}

	OutTargetComp = FinalTargetComp;

	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		static constexpr float DebugDrawDuration = 1.0f;
		OwnerCharacter->DrawDebugHomingArea( MyLoc, InDefaultDir, ResultDir, FinalTargetComp, MaxDist, OwnerCharacter->PlayerParamData->LightAttackHomingAngle, DebugDrawDuration,
			FColor::Cyan, OwnerCharacter->PlayerParamData->LightAttackHomingMaxHeightDiff );
	}

	return ResultDir;
}

void UAttackActionPlayerModule::SetupLightAttackApproach( ULockOnTargetComponent* TargetComp )
{
	LightAttackApproachScale = 1.0f;
	LightAttackApproachEndTime = 0.0f;

	if ( !TargetComp || !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	if ( !PlayerParams->bEnableLightAttackApproach ) return;

	// 空中の弱攻撃は振り下ろし（急降下）が移動を持つので触らない
	const UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp || MovementComp->IsFalling() ) return;

	// 判定発生までにルートモーションが進む距離＝この段の素のリーチ。
	// 倍率は距離を伸縮するだけなので、モンタージュの再生レートには依存しない
	const UAnimMontage* Montage = GetAnimMontage( GetLightAttackAnimTag() );
	const float TriggerTime = FindFirstHitWindowTriggerTime( Montage );
	if ( TriggerTime <= 0.0f ) return;

	const FAnimExtractContext ExtractContext;
	const float RootMotionDist = Montage->ExtractRootMotionFromTrackRange( 0.0f, TriggerTime, ExtractContext ).GetTranslation().Size2D();

	// その場攻撃（前進がほぼ無い段）は割ると倍率が発散するので対象外
	const float MinRootMotion = FMath::Max( KINDA_SMALL_NUMBER, PlayerParams->LightAttackApproachMinRootMotion );
	if ( RootMotionDist < MinRootMotion ) return;

	// 詰めたい距離＝吸着部位までの水平距離 − standoff。素の前進で足りていれば伸ばさない。
	// 「詰めるだけで離れない」＝縮み側（1.0 未満）は許さず、その場パンチ化を防ぐ
	const float TargetDistH = ( TargetComp->GetTargetLocation() - OwnerCharacter->GetActorLocation() ).Size2D();
	const float NeedDist = TargetDistH - FMath::Max( 0.0f, PlayerParams->LightAttackApproachStandoff );
	if ( NeedDist <= RootMotionDist ) return;

	const float MaxScale = FMath::Max( 1.0f, PlayerParams->LightAttackApproachMaxScale );
	LightAttackApproachScale = FMath::Clamp( NeedDist / RootMotionDist, 1.0f, MaxScale );
	LightAttackApproachEndTime = TriggerTime;
}

bool UAttackActionPlayerModule::IsFrontBlockedByPawn() const
{
	if ( !OwnerCharacter ) return false;

	const FVector StartLoc = OwnerCharacter->GetActorLocation();
	const FVector ForwardDir = OwnerCharacter->GetActorForwardVector();

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.0f;

	const float CheckDistance = 20.0f;
	const FVector EndLoc = StartLoc + ( ForwardDir * CheckDistance );

	FCollisionQueryParams Params;
	Params.AddIgnoredActor( OwnerCharacter );

	// 目の前へ球を飛ばして Pawn に当たるかを見る
	FHitResult Hit;
	const bool bHit = OwnerCharacter->GetWorld()->SweepSingleByChannel(
		Hit,
		StartLoc,
		EndLoc,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere( CapsuleRadius ),
		Params
	);

	if ( bHit && Hit.GetActor() && Hit.GetActor()->IsA<APawn>() )
	{
		return true;
	}

	return false;
}

#if !UE_BUILD_SHIPPING
void UAttackActionPlayerModule::DrawDebugImGui()
{
	if ( !ImGui::CollapsingHeader( "Attack Module", ImGuiTreeNodeFlags_DefaultOpen ) ) return;
	if ( !OwnerCharacter ) return;

	ImGui::Indent();

	const char* StateName = ( CurrentState == EAttackState::Idle ) ? "Idle" : ( CurrentState == EAttackState::Attacking ) ? "Attacking" : "Recovery";
	ImGui::Text( "State: %s", StateName );

	// マスク中は段数を見ても意味が無いので最初に出す
	const bool bMasked = OwnerCharacter->IsGroundNormalAttackMasked();
	ImGui::TextColored( bMasked ? ImVec4( 1, 0.4f, 0.4f, 1 ) : ImVec4( 0.4f, 1, 0.4f, 1 ),
		"地上弱攻撃: %s", bMasked ? "マスク中" : "有効" );

	ImGui::Separator();
	ImGui::Text( "コンボ段: %d / %d", CurrentLightComboIndex, GetMaxLightComboCount() );
	ImGui::Text( "モンタージュ: %s", TCHAR_TO_UTF8( *GetLightAttackAnimTag().ToString() ) );

	// 対象詰め。x1.00 なら「対象なし／素で届く／上限で頭打ち」のいずれか。
	// 窓は判定発生まで＝ここを過ぎると素の移動量に戻る
	const bool bInApproachWindow = LightAttackApproachScale > 1.0f && AttackTimer.GetElapsed() < LightAttackApproachEndTime;
	ImGui::TextColored( bInApproachWindow ? ImVec4( 0.4f, 0.8f, 1, 1 ) : ImVec4( 1, 1, 1, 1 ),
		"詰め倍率: x%.2f (窓 %.2f / %.2f)", LightAttackApproachScale, AttackTimer.GetElapsed(), LightAttackApproachEndTime );

	// 段飛び／段が伸びないときの切り分け。次段は CanCombo、最終段からの1段目復帰は CanAttack
	const bool bCanAttack = OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack );
	const bool bCanCombo = OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
	ImGui::Text( "受付タグ: CanAttack=%s / CanCombo=%s", bCanAttack ? "O" : "-", bCanCombo ? "O" : "-" );

	// 残留窓中に立っているタグは前段からの残りかす。ここが黄色い間の受付は弾いている
	const bool bInResidue = IsWithinLightAttackTagResidueWindow();
	const float ResidueTime = OwnerCharacter->PlayerParamData ? OwnerCharacter->PlayerParamData->LightAttackTagResidueIgnoreTime : 0.0f;
	ImGui::TextColored( bInResidue ? ImVec4( 1, 1, 0, 1 ) : ImVec4( 1, 1, 1, 1 ),
		"残留タグ窓: %s (%.2f / %.2f)", bInResidue ? "無視中" : "明け", AttackTimer.GetElapsed(), ResidueTime );

	ImGui::Separator();
	if ( !HitBackTimer.IsFinish() )
	{
		ImGui::Text( "ヒットバック: %.2f (%s) 速度 %.0f",
			HitBackTimer.GetElapsed(), bHitBackIsBreakthrough ? "前進" : "後退", CurrentHitBackVelocity.Size() );
	}
	if ( CurrentState != EAttackState::Idle ) ImGui::Text( "Attack Timer: %.2f", AttackTimer.GetElapsed() );

	ImGui::Unindent();
}
#endif
