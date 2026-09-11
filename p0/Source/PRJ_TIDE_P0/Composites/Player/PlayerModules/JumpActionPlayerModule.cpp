// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "JumpActionPlayerModule.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"

namespace
{
	// PlayerParamData 未設定時の風切りトレイル用ギア段階
	constexpr int32 FallbackWindTrailGear = 1;
}

void UJumpActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UJumpActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 風切りトレイルはジャンプ状態と独立した寿命を持つので、以降の早期 return より先に更新する
	UpdateWindTrailEffect( DeltaTime );

	// 空中チャージダッシュ中の死亡では CurrentJumpState が None のままなので、
	// 抜けないと下の崖落下検知が JUMP_LP を再生して死亡モーションを上書きしてしまう
	if ( OwnerCharacter->IsDead() )
	{
		CurrentJumpState = EJumpMontageState::None;
		return;
	}

	// CanJump タグ中は攻撃をジャンプでキャンセルできる。CanJump() は IsAttacking を弾くため先に専用処理する
	if ( OwnerCharacter->IsAttacking() &&
		OwnerCharacter->HasStateTag( TAG_State_Player_CanJump ) &&
		!OwnerCharacter->HasStateTag( TAG_State_Player_CannotJump ) )
	{
		const float BufferTime = OwnerCharacter->PlayerParamData->JumpBufferTime;
		if ( TryConsumeCommand( TAG_Input_Command_Jump, BufferTime ) )
		{
			OwnerCharacter->TryJumpCancelDuringAction();
			return;
		}
	}

	if ( CanJump() )
	{
		const float BufferTime = OwnerCharacter->PlayerParamData->JumpBufferTime;
		if ( TryConsumeCommand( TAG_Input_Command_Jump, BufferTime ) )
		{
			ExecuteJump();
		}
	}

	// 別のアクションが割り込んでいる間はジャンプアニメの制御を中断する。中断しないと空中落下検知が
	// JUMP_LP を強制再生して相手のモーションを上書きし、着地時に誤って JUMP_ED も出る
	if ( OwnerCharacter->IsAttacking() ||
		OwnerCharacter->IsDodging() ||
		OwnerCharacter->IsPlayingChargeAction() ||
		OwnerCharacter->IsCharging() ||
		OwnerCharacter->IsHitReacting() ||
		OwnerCharacter->IsBoostDashing() ||
		OwnerCharacter->IsInGlideSession() )
	{
		CurrentJumpState = EJumpMontageState::None;
		return;
	}

	// 崖からの歩き落下検知（ジャンプを経由しない落下・他モジュールからの引き継ぎ）。チャージホップは着地までの
	// 一連を V2 が自己完結で管理するため、ここで Loop 状態を奪うと CHARGE_HOP_ED が JUMP_ED に化ける
	if ( CurrentJumpState == EJumpMontageState::None && OwnerCharacter->IsFalling() && !OwnerCharacter->IsPlayingChargeHopJump() )
	{
		if ( OwnerCharacter->GetVelocity().Z <= 0.0f )
		{
			UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
			UAnimMontage* JumpLpMontage = GetAnimMontage( PlayerAnimTags::JUMP_LP );
			UAnimMontage* ChargeJumpStMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST );
			UAnimMontage* ChargeJumpLpMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP );

			// ST が最後まで再生されるのを待つため、ST 再生中も LP による上書きを防ぐ
			if ( CurrentMontage != JumpLpMontage &&
				CurrentMontage != ChargeJumpLpMontage &&
				CurrentMontage != ChargeJumpStMontage )
			{
				PlayAnimMontage( PlayerAnimTags::JUMP_LP );
			}

			CurrentJumpState = EJumpMontageState::Loop;	// 管轄下に入るので着地時の吸収が効く
		}
	}

	// ST -> LP への自動遷移
	if ( CurrentJumpState == EJumpMontageState::Start && OwnerCharacter->IsFalling() )
	{
		// 竜巻ジャンプ・吹き飛び復帰ジャンプはチャージジャンプの ST/LP で遷移する
		const bool bUseChargeJumpMotion = bWindJumpMotion || bForceChargeJumpMotion;
		const FName StartTag = bUseChargeJumpMotion ? PlayerAnimTags::CHARGE_JUMP_ST : PlayerAnimTags::JUMP_ST;
		const FName LoopTag  = bUseChargeJumpMotion ? PlayerAnimTags::CHARGE_JUMP_LP : PlayerAnimTags::JUMP_LP;

		UAnimMontage* JumpStartMontage = GetAnimMontage( StartTag );
		bool bIsMontageFinished = false;

		if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
		{
			if ( OwnerCharacter->GetCurrentMontage() != JumpStartMontage )
			{
				bIsMontageFinished = true;
			}
			else
			{
				const float CurrentPos = AnimInst->Montage_GetPosition( JumpStartMontage );
				const float Length = JumpStartMontage->GetPlayLength();

				// 末尾で停止してポーズ保持しているケースに対応（残り 0.05 秒で終了とみなす）
				if ( CurrentPos >= ( Length - 0.05f ) )
				{
					bIsMontageFinished = true;
				}
			}
		}

		if ( bIsMontageFinished )
		{
			PlayAnimMontage( LoopTag );
			CurrentJumpState = EJumpMontageState::Loop;
		}
	}

	// 着地(ED)モーション中に移動入力を検知したら、モーションをキャンセルして走らせる
	if ( CurrentJumpState == EJumpMontageState::End && !OwnerCharacter->IsFalling() )
	{
		// アニメーション側から MoveCancelable タグが付与されるまでは保留する
		if ( OwnerCharacter->HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			if ( OwnerCharacter->GetRawMovementInput().SizeSquared() > 0.01f )
			{
				if ( bReserveAutoDashOnLanding )
				{
					bReserveAutoDashOnLanding = false;
					OwnerCharacter->ForceStartDash();
				}

				if ( UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage() )
				{
					OwnerCharacter->StopAnimMontage( CurrentMontage );
				}

				CurrentJumpState = EJumpMontageState::None;
			}
		}

		// 入力せずに ED が最後まで再生し終わった場合のステートリセット
		if ( UAnimInstance* AnimInst = OwnerCharacter->GetMesh()->GetAnimInstance() )
		{
			UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
			UAnimMontage* JumpEdMontage = GetAnimMontage( PlayerAnimTags::JUMP_ED );
			UAnimMontage* ChargeJumpEdMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ED );

			if ( CurrentMontage != JumpEdMontage && CurrentMontage != ChargeJumpEdMontage )
			{
				CurrentJumpState = EJumpMontageState::None;
			}
		}
	}
}

void UJumpActionPlayerModule::RequestJump()
{
	if ( !CanJump() ) return;

	ExecuteJump();
}

void UJumpActionPlayerModule::ForceJump( bool bOverrideXY )
{
	if ( !OwnerCharacter ) return;

	// 空中から飛ぶケースが基本なのでジャンプ回数をリセットしてから委ねる
	ResetJumpCount();

	// 吹き飛びキャンセル（水平速度を断ち切るケース）は CanMove タグが付かない状態から飛ぶため、
	// 着地までの間は空中横移動を特別に許可する。あわせて復帰ジャンプはチャージジャンプのモーションで流す
	bHitCancelAirMove = bOverrideXY;
	bForceChargeJumpMotion = bOverrideXY;

	ExecuteJump( bOverrideXY );
}

void UJumpActionPlayerModule::OnLanded()
{
	if ( !OwnerCharacter ) return;

	ResetJumpCount();

	StopWindTrailEffect();

	// 吹き飛びキャンセルジャンプの空中移動許可・復帰ジャンプモーション指定を解除する
	bHitCancelAirMove = false;
	bForceChargeJumpMotion = false;
	bHasJumpedThisAirtime = false;

	// 死亡中は ED 再生・オートダッシュを行わず（死亡モーションを上書きしないため）後始末だけして抜ける
	if ( OwnerCharacter->IsDead() )
	{
		bReserveAutoDashOnLanding = false;
		CurrentJumpState = EJumpMontageState::None;
		OwnerCharacter->RequestPopFirstJumpCamera();
		return;
	}

	// 次回の暴発を防ぐため、予約はローカルへ退避して即座に破棄する
	const bool bCanAutoDash = bReserveAutoDashOnLanding;
	bReserveAutoDashOnLanding = false;

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* JumpLoopMontage = GetAnimMontage( PlayerAnimTags::JUMP_LP );
	UAnimMontage* JumpStartMontage = GetAnimMontage( PlayerAnimTags::JUMP_ST );

	UAnimMontage* ChargeJumpStartMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST );
	UAnimMontage* ChargeJumpLoopMontage = GetAnimMontage( PlayerAnimTags::CHARGE_JUMP_LP );

	const bool bIsJumpMontagePlaying = ( CurrentMontage != nullptr ) &&
		( CurrentMontage == JumpLoopMontage || CurrentMontage == JumpStartMontage ||
			CurrentMontage == ChargeJumpLoopMontage || CurrentMontage == ChargeJumpStartMontage );

	// ステートがジャンプ中、またはジャンプのモーションが再生中なら着地を吸収する
	if ( CurrentJumpState == EJumpMontageState::Start ||
		CurrentJumpState == EJumpMontageState::Loop ||
		bIsJumpMontagePlaying )
	{
		// JUMP_LP が流れている＝実質ただの落下状態なので、裏に残った回避・攻撃のタイマーは強制終了させる
		if ( OwnerCharacter->IsDodging() )
		{
			OwnerCharacter->CancelDodge();
		}
		if ( OwnerCharacter->IsAttacking() )
		{
			OwnerCharacter->CancelAttack();
		}

		if ( !OwnerCharacter->IsAttacking() &&
			!OwnerCharacter->IsDodging() &&
			!OwnerCharacter->IsCharging() )
		{
			// 入力の有無に関わらず、まずは ED を再生して End へ移行する
			if ( CurrentMontage == ChargeJumpLoopMontage || CurrentMontage == ChargeJumpStartMontage )
			{
				PlayAnimMontage( PlayerAnimTags::CHARGE_JUMP_ED );
			}
			else
			{
				PlayAnimMontage( PlayerAnimTags::JUMP_ED );
			}

			CurrentJumpState = EJumpMontageState::End;

			// ダッシュ予約は ED 明けまで保留する
			if ( bCanAutoDash )
			{
				bReserveAutoDashOnLanding = true;
			}
		}
		else
		{
			// ED を流さずに手放すケース（チャージ中の着地）。ステートを None にするだけだと ST／LP が再生され
			// 続けて末尾のポーズで硬直する（ST→LP は IsFalling() 条件、ED は本関数が持ち主なので誰も止めない）
			if ( bIsJumpMontagePlaying )
			{
				OwnerCharacter->StopAnimMontage( CurrentMontage );
			}
			CurrentJumpState = EJumpMontageState::None;
		}
	}
	else
	{
		CurrentJumpState = EJumpMontageState::None;
	}

	OwnerCharacter->RequestPopFirstJumpCamera();
}

void UJumpActionPlayerModule::EnterFallingLoop()
{
	if ( !OwnerCharacter ) return;

	bWindJumpMotion = false;
	bForceChargeJumpMotion = false;

	// JUMP_LP が既に流れている場合は再生し直さない
	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	UAnimMontage* JumpLpMontage = GetAnimMontage( PlayerAnimTags::JUMP_LP );
	if ( CurrentMontage != JumpLpMontage )
	{
		PlayAnimMontage( PlayerAnimTags::JUMP_LP );
	}

	CurrentJumpState = EJumpMontageState::Loop;	// 以降は OnLanded が ED へ繋ぐ
}

void UJumpActionPlayerModule::EnterChargeJumpStart()
{
	if ( !OwnerCharacter ) return;

	// ST から流す。呼び出し側で LP を直接張ると ST が飛ぶうえ、遷移の持ち主が居なくなる
	bWindJumpMotion = false;
	bForceChargeJumpMotion = true;

	PlayAnimMontage( PlayerAnimTags::CHARGE_JUMP_ST );
	CurrentJumpState = EJumpMontageState::Start;
}

void UJumpActionPlayerModule::EnterLandingEnd()
{
	if ( !OwnerCharacter ) return;

	// 以降は OnModuleUpdate の End 分岐が ED キャンセル（走り出し）・自然終了のリセットを担う
	PlayAnimMontage( PlayerAnimTags::JUMP_ED );
	CurrentJumpState = EJumpMontageState::End;
}

void UJumpActionPlayerModule::ConsumeAirJumps()
{
	if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
	{
		// 最大値にすることで着地するまで通常のジャンプを封じる
		CurrentJumpCount = OwnerCharacter->PlayerParamData->MaxJumpCount;
	}
}

void UJumpActionPlayerModule::StartWindTrailEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	// チャージダッシュ中は同じエフェクトをチャージ側が出しているので二重に出さない
	if ( OwnerCharacter->IsPlayingChargeDash() ) return;

	StopWindTrailEffect();

	UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGE_DASH_WIND );
	if ( !EffectSys ) return;

	const UTidePlayerParamDataAsset* PlayerParams = OwnerCharacter->PlayerParamData;
	const float ForwardOffset = PlayerParams ? PlayerParams->ChargeDashWindEffectForwardOffset : 200.0f;

	SpawnedWindTrailEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		EffectSys,
		OwnerCharacter->GetRootComponent(),
		NAME_None,
		FVector( ForwardOffset, 0.0f, 0.0f ),	// プレイヤー前方へオフセット
		FRotator( 0.0f, 180.0f, 0.0f ),			// Yaw をプレイヤー＋180（逆向き）
		EAttachLocation::SnapToTarget,
		true
	);
	if ( !SpawnedWindTrailEffect ) return;

	const int32 Gear = PlayerParams ? FMath::Clamp( PlayerParams->JumpWindTrailGear, 1, 3 ) : FallbackWindTrailGear;
	SpawnedWindTrailEffect->SetIntParameter( TEXT( "Gear" ), Gear );

	// 0以下ならタイマー無効（IsValid が false）＝着地まで流し続ける
	WindTrailTimer.Set( PlayerParams ? FMath::Max( 0.0f, PlayerParams->JumpWindTrailDuration ) : 0.0f );
}

void UJumpActionPlayerModule::StopWindTrailEffect()
{
	WindTrailTimer.Clear();

	if ( SpawnedWindTrailEffect )
	{
		SpawnedWindTrailEffect->DestroyComponent();
		SpawnedWindTrailEffect = nullptr;
	}
}

void UJumpActionPlayerModule::UpdateWindTrailEffect( float DeltaTime )
{
	if ( !SpawnedWindTrailEffect ) return;

	// 死亡と、チャージ側が同じエフェクトを出すケースは即消す
	if ( OwnerCharacter->IsDead() || OwnerCharacter->IsPlayingChargeDash() )
	{
		StopWindTrailEffect();
		return;
	}

	if ( WindTrailTimer.IsValid() && WindTrailTimer.Update( DeltaTime ) <= 0.0f )
	{
		StopWindTrailEffect();
	}
}

bool UJumpActionPlayerModule::CanJump() const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return false;

	// ジャンプパッド打ち上げ中はジャンプのみ封印（他アクションは可能）。頂点/着地で解除される
	if ( OwnerCharacter->HasStateTag( TAG_State_Player_CannotJump ) ) return false;

	// CanMove（移動＋ジャンプ）または CanJump（ジャンプだけ先行解禁）のいずれかで許可する
	if ( !OwnerCharacter->HasStateTag( TAG_State_Player_CanMove ) &&
		!OwnerCharacter->HasStateTag( TAG_State_Player_CanJump ) ) return false;

	if ( OwnerCharacter->IsAttacking() ||
		//OwnerCharacter->IsDodging() ||
		OwnerCharacter->IsHitReacting() ||
		OwnerCharacter->IsPlayingChargeJump() ||
		OwnerCharacter->IsBoostDashing() ||
		OwnerCharacter->IsAirChargeDashing() ) return false;

	if ( !OwnerCharacter->IsFalling() ) return true;

	return ( CurrentJumpCount < OwnerCharacter->PlayerParamData->MaxJumpCount );
}

void UJumpActionPlayerModule::ExecuteJump( bool bOverrideXY )
{
 	if ( !OwnerCharacter ) return;

	OwnerCharacter->CancelAttack();
	OwnerCharacter->CancelDodge();

	// 竜巻内にいるときはジャンプ力を強化し、モーション遷移もチャージジャンプのものを使う
	const float WindJumpBoost = OwnerCharacter->GetWindJumpBoostMultiplier();
	bWindJumpMotion = ( WindJumpBoost > 1.0f );

	// 吹き飛びキャンセルジャンプ（bOverrideXY）は水平速度を断ち切るため、必ず LaunchCharacter 経路を使う
	if ( CurrentJumpCount == 0 && !bWindJumpMotion && !OwnerCharacter->IsFalling() && !bOverrideXY )
	{
		OwnerCharacter->Jump();	// 地上からの 1 段目は UE 標準
	}
	else
	{
		if ( UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement() )
		{
			// 竜巻ジャンプは専用の基準初速を使う。多段・吹き飛び復帰は通常ジャンプ力のまま
			float BaseJumpZ = Movement->JumpZVelocity;
			if ( bWindJumpMotion && OwnerCharacter->PlayerParamData )
			{
				BaseJumpZ = OwnerCharacter->PlayerParamData->WindJumpZVelocity;
			}
			float JumpZ = BaseJumpZ * FMath::Max( 1.0f, WindJumpBoost );

			// 吹き飛び復帰ジャンプはジャンプ力を抑える
			if ( bForceChargeJumpMotion && OwnerCharacter->PlayerParamData )
			{
				JumpZ *= OwnerCharacter->PlayerParamData->RecoveryJumpPowerRate;
			}

			OwnerCharacter->LaunchCharacter( FVector( 0.0f, 0.0f, JumpZ ), bOverrideXY, true );
		}
	}

	const bool bUseChargeJumpMotion = bWindJumpMotion || bForceChargeJumpMotion;

	// 復帰ジャンプ ST は専用の再生速度を使う
	float StartPlayRate = 1.0f;
	if ( bForceChargeJumpMotion && OwnerCharacter->PlayerParamData )
	{
		StartPlayRate = OwnerCharacter->PlayerParamData->RecoveryJumpStPlayRate;
	}

	PlayAnimMontage( bUseChargeJumpMotion ? PlayerAnimTags::CHARGE_JUMP_ST : PlayerAnimTags::JUMP_ST, StartPlayRate );

	// 風切りトレイルは竜巻ジャンプでのみ出す
	if ( bWindJumpMotion )
	{
		StartWindTrailEffect();
	}

	CurrentJumpCount++;
	bHasJumpedThisAirtime = true;

	OwnerCharacter->RequestPushFirstJumpCamera();

	CurrentJumpState = EJumpMontageState::Start;
}

void UJumpActionPlayerModule::ResetJumpCount()
{
	CurrentJumpCount = 0;
}
