// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "HitReactionPlayerModule.h"

#include "NiagaraFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

void UHitReactionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UHitReactionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const float RealDeltaTime = OwnerCharacter->GetWorld()->GetDeltaSeconds();

	if ( !InvincibilityTimer.IsFinish() )
	{
		InvincibilityTimer.Update( RealDeltaTime );

		if( InvincibilityTimer.IsFinish() )
		{
			OwnerCharacter->RemoveStateTag( TAG_State_Common_HitReaction_Immune );
		}
	}

	// 被弾時間と同じく実時間で計測する
	if ( !BlowbackCancelTimer.IsFinish() )
	{
		BlowbackCancelTimer.Update( RealDeltaTime );
	}

	if ( CurrentState == EHitReactionState::Idle ) return;

	UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance();
	if ( !AnimInst ) return;

	switch ( CurrentState )
	{
	case EHitReactionState::Flinching:
	{
		UAnimMontage* FlinchMontage = GetAnimMontage( CurrentFlinchMontageName );

		// 硬直明け（CanMove + MoveCancelable）の区間は移動／回避入力で早期キャンセルできる
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) &&
			OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			if ( OwnerCharacter->GetRawMovementInput().SizeSquared() > 0.01f ||
				OwnerCharacter->HasDodgeInputBuffered() )
			{
				if ( OwnerCharacter->GetCurrentMontage() == FlinchMontage )
				{
					OwnerCharacter->StopAnimMontage( FlinchMontage );
				}
				// 攻撃者を向いたまま復帰すると正面入力で Turn が誤発火するため、入力方向へ向き直す
				OrientForRecoveryExit();
				ClearState();
				break;
			}
		}

		if ( OwnerCharacter->GetCurrentMontage() != FlinchMontage )
		{
			ClearState();
		}
		break;
	}

	case EHitReactionState::Blowback_Start:
	{
		if ( TryBlowbackCancel() ) break;

		// ST（上昇）が終わったら、接地済みなら ED、まだ空中なら LP へ
		UAnimMontage* StMontage = GetAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_ST );

		if ( IsMontageFinished( StMontage ) )
		{
			if ( !OwnerCharacter->IsFalling() )
			{
				PlayAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_ED );
				CurrentState = EHitReactionState::Blowback_End;
				OwnerCharacter->StopVelocity();	// 着地後に滑るのを防ぐ
				break;
			}

			PlayAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_LP );
			CurrentState = EHitReactionState::Blowback_Loop;
		}
		break;
	}

	case EHitReactionState::Blowback_Loop:
	{
		if ( TryBlowbackCancel() ) break;

		// 着地したら ED（ダウン着地）へ
		if ( !OwnerCharacter->IsFalling() )
		{
			PlayAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_ED );
			CurrentState = EHitReactionState::Blowback_End;
			OwnerCharacter->StopVelocity();	// 着地後に滑るのを防ぐ
		}
		break;
	}

	case EHitReactionState::Blowback_End:
	{
		// ED（ダウン）が終わったら起き上がりへ
		UAnimMontage* EdMontage = GetAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_ED );

		bool bEarlyCancel = false;
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			if ( OwnerCharacter->HasRecoveryInput() )
			{
				bEarlyCancel = true;
			}
		}

		if ( IsMontageFinished( EdMontage ) || bEarlyCancel )
		{
			PlayAnimMontage( PlayerAnimTags::RECOVERY_DOWN_U );
			CurrentState = EHitReactionState::Recovering;
		}
		break;
	}

	case EHitReactionState::Recovering:
	{
		UAnimMontage* RecMontage = GetAnimMontage( PlayerAnimTags::RECOVERY_DOWN_U );

		if ( OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) &&
			OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			if ( OwnerCharacter->GetRawMovementInput().SizeSquared() > 0.01f ||
				OwnerCharacter->HasDodgeInputBuffered() )
			{
				if ( OwnerCharacter->GetCurrentMontage() == RecMontage )
				{
					OwnerCharacter->StopAnimMontage( RecMontage );
				}
				OrientForRecoveryExit();
				ClearState();
				break;
			}
		}

		if ( IsMontageFinished( RecMontage ) )
		{
			OrientForRecoveryExit();
			ClearState();
		}
		break;
	}
	}
}

void UHitReactionPlayerModule::ProcessHit( FGameplayTag ReactionTag, AActor* Instigator )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// のけぞり／吹き飛びを出さないだけで、ダメージ・VFX 自体は別経路で通る
	if ( !ReactionTag.IsValid() || ReactionTag == TAG_HitReaction_NoReaction ) return;

	if ( IsInvincible() ) return;

	const float InvincibilityTime = OwnerCharacter->PlayerParamData->HitInvincibilityTime;
	if ( InvincibilityTime > 0.0f )
	{
		InvincibilityTimer.Set( InvincibilityTime );
		OwnerCharacter->AddStateTag( TAG_State_Common_HitReaction_Immune );
	}

	// 神技の構えは先に解除する（スロー・重力・移動パラメータを戻してから下の一括停止へ渡す）。
	// 復帰には L2 の押し直しが必要で、押しっぱなしでは構えへ戻らない
	OwnerCharacter->NotifyGodArtStanceInterruptedByDamage();

	OwnerCharacter->StopAllMovementAndInputs();

	// 被弾で失った溜めは R2 長押しのままでは再開させない（DA の部分適用フラグが ON のときだけ）
	OwnerCharacter->NotifyChargeInterruptedByDamage();

	// 攻撃者を向く。これで吹き飛びの後方ノックバック（-Forward）も攻撃者から離れる向きになる
	if ( Instigator )
	{
		FVector ToInstigator = Instigator->GetActorLocation() - OwnerCharacter->GetActorLocation();
		ToInstigator.Z = 0.0f;
		if ( !ToInstigator.IsNearlyZero() )
		{
			OwnerCharacter->SetActorRotation( ToInstigator.GetSafeNormal().Rotation() );
		}
	}

	// 吹き飛び → ダウンシーケンス（ST→LP→ED→起き上がり）
	if ( ReactionTag.MatchesTag( TAG_HitReaction_Blowoff ) )
	{
		CurrentState = EHitReactionState::Blowback_Start;
		BlowbackCancelTimer.Set( OwnerCharacter->PlayerParamData->BlowbackCancelEnableTime );
		PlayAnimMontage( PlayerAnimTags::BLOWDAMAGE_F_ST );

		const FVector KnockbackDir = -OwnerCharacter->GetActorForwardVector();
		const float HorizPower = OwnerCharacter->PlayerParamData->BlowbackHorizontalPower;
		const float VertPower  = OwnerCharacter->PlayerParamData->BlowbackVerticalPower;

		OwnerCharacter->LaunchCharacter( ( KnockbackDir * HorizPower ) + FVector( 0.0f, 0.0f, VertPower ), true, true );
		return;
	}

	// のけぞりは強度で小／中／大を選ぶ。後ろのけぞり（*.Back）は専用モーション未実装のため前で代用する
	FName MontageName = PlayerAnimTags::STAGGER_M_F;
	if ( ReactionTag.MatchesTag( TAG_HitReaction_Knockback_S ) )
	{
		MontageName = PlayerAnimTags::STAGGER_S_F;
	}
	else if ( ReactionTag.MatchesTag( TAG_HitReaction_Knockback_L ) )
	{
		MontageName = PlayerAnimTags::STAGGER_L_F;
	}
	else if ( ReactionTag.MatchesTag( TAG_HitReaction_Knockback_M ) )
	{
		MontageName = PlayerAnimTags::STAGGER_M_F;
	}

	// モーション未割当だと Flinching の終了判定が nullptr 同士で成立せず、抜けられず操作不能になる
	if ( !GetAnimMontage( MontageName ) )
	{
		return;
	}

	CurrentState = EHitReactionState::Flinching;
	CurrentFlinchMontageName = MontageName;
	PlayAnimMontage( MontageName );
}

bool UHitReactionPlayerModule::IsInvincible() const
{
	if ( !OwnerCharacter ) return false;
	return OwnerCharacter->HasStateTag( TAG_State_Common_Invincible ) ||
		!InvincibilityTimer.IsFinish() ||
		IsImmune() ||
		UTideGameSettings::Get()->bDebugFlagInvincible;

}

bool UHitReactionPlayerModule::IsImmune() const
{
	if ( !OwnerCharacter ) return false;
	return OwnerCharacter->HasStateTag( TAG_State_Common_HitReaction_Immune );
}

bool UHitReactionPlayerModule::TryBlowbackCancel()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;

	if ( !BlowbackCancelTimer.IsFinish() ) return false;	// 受付開始前

	// ジャンプ優先
	if ( TryConsumeCommand( TAG_Input_Command_Jump, OwnerCharacter->PlayerParamData->JumpBufferTime ) )
	{
		ClearState();
		OwnerCharacter->ForceJump( true );	// 水平速度も上書きして後方への吹き飛びを断ち切る
		// 被弾で中断したアクションの移動パラメータ（AirControl 等）が残るので通常値へ戻す
		OwnerCharacter->RefreshMovementParams();
		return true;
	}

	if ( TryConsumeCommand( TAG_Input_Command_Dodge, OwnerCharacter->PlayerParamData->DodgeBufferTime ) )
	{
		ClearState();
		OwnerCharacter->ForceDodge();
		return true;
	}

	return false;
}

void UHitReactionPlayerModule::OrientForRecoveryExit()
{
	if ( !OwnerCharacter ) return;

	// 入力があればその方向、無ければカメラ正面へ揃える
	FVector Dir = FVector::ZeroVector;
	const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
	if ( !RawInput.IsNearlyZero() )
	{
		Dir = OwnerCharacter->GetControlRelativeInputDirection( RawInput ).GetSafeNormal2D();
	}
	else
	{
		Dir = FRotator( 0.0f, OwnerCharacter->GetControlRotation().Yaw, 0.0f ).Vector();
	}

	if ( !Dir.IsNearlyZero() )
	{
		OwnerCharacter->SetActorRotation( Dir.Rotation() );
	}

	OwnerCharacter->StopVelocity();	// 残留速度で Turn が誤発火しないようにする

	// root motion が SetActorRotation/StopVelocity を上書きしても、このウィンドウ中は
	// Turn 抑制＋入力方向移動で暴発を防ぐ
	OwnerCharacter->NotifyHitReactionRecovered();
}

void UHitReactionPlayerModule::ClearState()
{
	CurrentState = EHitReactionState::Idle;
	BlowbackCancelTimer.Clear();
}

bool UHitReactionPlayerModule::IsMontageFinished( UAnimMontage* Montage ) const
{
	if ( !OwnerCharacter || !Montage ) return true;

	UAnimInstance* AnimInst = OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if ( !AnimInst ) return true;

	// 別モーションへ切替わった／停止した／末尾に到達したいずれかで終了とみなす
	if ( OwnerCharacter->GetCurrentMontage() != Montage ) return true;
	if ( !AnimInst->Montage_IsPlaying( Montage ) ) return true;
	return AnimInst->Montage_GetPosition( Montage ) >= Montage->GetPlayLength();
}

#if !UE_BUILD_SHIPPING
void UHitReactionPlayerModule::DrawDebugImGui()
{

}
#endif
