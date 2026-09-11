// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "ChargeActionPlayerModule.h"

#include <imgui.h>
#include "Math/UnrealMathUtility.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

// 削除予定
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

void UChargeActionPlayerModule::Initialize( ATidePlayerCharacter* InOwner )
{
	Super::Initialize( InOwner );
}

void UChargeActionPlayerModule::OnModuleUpdate( float DeltaTime )
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams ) return;
	if ( PlayerParams->bUseChargeV2 ) return;

	UpdateComboReset();
	UpdateChargeJumpLanding();
	UpdateGhostTrails( DeltaTime );
	UpdateChargeDashMontageState();

	if ( !UpdateInputCommands() )
	{
		UpdateChargeBeginCheck();
	}
	UpdateChargingState( DeltaTime );
	UpdatePropulsionLock( DeltaTime );
	UpdateFrictionRecoveryState( DeltaTime );
	UpdateChargeDashCameraPop( DeltaTime );
}

void UChargeActionPlayerModule::UpdateComboReset()
{
	if ( CurrentChargeComboIndex <= 1 )
	{
		return;
	}

	if ( !bIsCharging && !IsPlayingChargeAction() && !OwnerCharacter->IsAttacking() )
	{
		CurrentChargeComboIndex = 1;
	}
}

void UChargeActionPlayerModule::UpdateChargeJumpLanding()
{
	if ( CurrentChargeActionType != EChargeActionType::Jump || !ChargedActionLockTimer.IsFinish() )
	{
		return;
	}

	if ( OwnerCharacter && !OwnerCharacter->IsFalling() )
	{
		OnEndAction();
	}
}

void UChargeActionPlayerModule::UpdateChargeDashMontageState()
{
	if ( !IsPlayingChargeDash() )
	{
		return;
	}

	UAnimMontage* DashMontage = GetAnimMontage( PlayerAnimTags::DASH );
	if ( !DashMontage )
	{
		return;
	}

	if ( OwnerCharacter->IsFalling() )
	{
		if ( OwnerCharacter->GetCurrentMontage() == DashMontage )
		{
			OwnerCharacter->StopAnimMontage();
		}
		return;
	}

	if ( OwnerCharacter->GetCurrentMontage() != DashMontage )
	{
		PlayAnimMontage( PlayerAnimTags::DASH );
	}
}

void UChargeActionPlayerModule::UpdateChargeDashCameraPop( float DeltaTime )
{
	if ( ChargeDashCameraPopTimer.IsFinish() )
	{
		return;
	}

	ChargeDashCameraPopTimer.Update( DeltaTime );
	if ( !ChargeDashCameraPopTimer.IsFinish() )
	{
		return;
	}

	if ( ChargeDashCameraHandle.IsValid() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			if ( CameraSubsystem->PopCameraMode( ChargeDashCameraHandle, TEXT( "PopChargeDash" ) ) )
			{
				ChargeDashCameraHandle.Clear();
			}
		}
	}

	ChargeDashCameraPopTimer.Clear();
}

bool UChargeActionPlayerModule::UpdateInputCommands()
{
	bool bActionDispatched = false;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams )
	{
		return false;
	}

	if ( TryConsumeCommand( TAG_Input_Command_ChargeAction, PlayerParams->ChargeBufferTime ) )
	{
		bChargeInputHeld = true;
	}

	// チャージ中・突進中の攻撃コマンド横取り
	const bool bIsProcessingCharge = bIsCharging || IsPlayingChargeAction();
	if ( bIsProcessingCharge && OwnerCharacter->HasStateTag( TAG_State_Player_CanAttack ) )
	{
		if ( TryConsumeCommand( TAG_Input_Command_Attack, PlayerParams->AttackBufferTime ) )
		{
			if ( RequestChargeAttack() )
			{
				bActionDispatched = true;
			}
		}
	}

	return bActionDispatched;
}

void UChargeActionPlayerModule::UpdateChargeBeginCheck()
{
	// チャージアクション専用ロック期間中は新たなチャージ開始をブロックする
	// （チャージダッシュ突進中の攻撃派生要求のみ例外として許可）
	if ( IsPlayingChargeAction() && !IsPlayingChargeDash() )
	{
		return;
	}

	const bool bIsInputValid = bChargeInputHeld && !bIsCharging;
	const bool bIsComboWindowActive = OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );
	const bool bCanProceedCombo = bIsComboWindowActive && ( CurrentChargeComboIndex < MaxChargeComboCount );
	const bool bIsAttacking = OwnerCharacter->IsAttacking();

	// 攻撃中でも bIsStateValid 側でタグをチェックするため、ここでは自由とみなす
	const bool bIsActionFree = !IsPlayingChargeAction() || IsPlayingChargeDash() || bIsAttacking || bCanProceedCombo;

	// コンボ継続ルートに乗っていれば CanCharge タグがなくても移行を許可する
	const bool bIsStateValid = !OwnerCharacter->HasStateTag( TAG_State_Common_Disable ) &&
		( OwnerCharacter->HasStateTag( TAG_State_Player_CanCharge ) || bCanProceedCombo ) &&
		!OwnerCharacter->IsHitReacting();

	if ( bIsInputValid && bIsActionFree && bIsStateValid )
	{
		BeginCharge();
	}
}

void UChargeActionPlayerModule::UpdateChargingState( float DeltaTime )
{
	if ( !bIsCharging ) return;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams )
	{
		return;
	}

	CurrentChargeTimer.Update( DeltaTime );

	const float ChargeRate = CurrentChargeTimer.GetRate();
	OwnerCharacter->UpdateChargingMovementParams( ChargeRate );

	if ( UTideGameSettings::Get()->bDebugFlagDrawHomingArea )
	{
		// 毎フレーム更新するため、生存時間 0.0f（1フレームのみ）で内部描画だけを走らせる
		constexpr float PreviewDuration = 0.0f;
		GetHomingDirection( GetActorYawForwardDirection(), PreviewDuration );
	}

	if ( !bHasReachedMaxCharge && CurrentChargeTimer.IsFinish() )
	{
		bHasReachedMaxCharge = true;
		PlayAnimMontage( GetChargeLoopAnimTag() );
	}

	if ( APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
	{
		float MinChargeTime = PlayerParams->MinChargeTimeForDash;
		if ( CurrentChargeComboIndex >= 2 ) MinChargeTime = 0.0f;
		DrawChargeGaugeUI(
			CurrentChargeTimer.GetElapsed(),
			CurrentChargeTimer.GetStart(),
			MinChargeTime,
			OwnerCharacter->GetActorLocation(),
			PC
		);
	}
}

void UChargeActionPlayerModule::UpdatePropulsionLock( float DeltaTime )
{
	if ( ChargedActionLockTimer.IsFinish() ) return;

	ChargedActionLockTimer.Update( DeltaTime );

	if ( CurrentChargeActionType == EChargeActionType::Dash )
	{
		if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			FVector CurrentDir = OwnerCharacter->GetActorForwardVector();

			if ( MovementComp->IsMovingOnGround() )
			{
				const FVector FloorNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				CurrentDir = FVector::VectorPlaneProject( CurrentDir, FloorNormal ).GetSafeNormal();
			}

			const FVector NewVelocity = CurrentDir * CachedChargeDashSpeed;
			MovementComp->Velocity.X = NewVelocity.X;
			MovementComp->Velocity.Y = NewVelocity.Y;
		}
	}

	if ( ChargedActionLockTimer.IsFinish() )
	{
		// ジャンプかつ空中ならここでは終了させず維持する
		if ( CurrentChargeActionType == EChargeActionType::Jump && OwnerCharacter->IsFalling() )
		{
			return;
		}
		OnEndAction();
	}
}

void UChargeActionPlayerModule::UpdateFrictionRecoveryState( float DeltaTime )
{
	if ( FrictionRecoveryTimer.IsFinish() ) return;

	FrictionRecoveryTimer.Update( DeltaTime );
	if ( OwnerCharacter != nullptr )
	{
		OwnerCharacter->UpdateFrictionRecovery( FrictionRecoveryTimer.GetRate() );
	}
}

void UChargeActionPlayerModule::BeginCharge()
{
	bChargeInputHeld = true;

	if ( !OwnerCharacter ) return;
	UpdateChargeComboIndex();
	InterruptCurrentChargeAction();
	PrepareOwnerForCharge();
	StartChargeState();
	PlayChargeStartMontage();
	SpawnChargeEffect();
	ApplyChargeStartMovement();
}

void UChargeActionPlayerModule::ReleaseCharge()
{
	bChargeInputHeld = false;

	if ( !bIsCharging ) return;

	if ( OwnerCharacter &&
		OwnerCharacter->PlayerParamData &&
		OwnerCharacter->PlayerParamData->bEnableChargeReleaseAttack )
	{
		RequestChargeAttack();
	}
	else
	{
		RequestChargeDash();
	}

	bIsCharging = false;
}

bool UChargeActionPlayerModule::IsPlayingChargeAction() const
{
	if ( !ChargedActionLockTimer.IsFinish() ) return true;

	// ジャンプのみ着地するまで「プレイ中」とみなす
	if ( CurrentChargeActionType == EChargeActionType::Jump )
	{
		if ( OwnerCharacter && OwnerCharacter->IsFalling() )
		{
			return true;
		}
	}

	return false;
}

bool UChargeActionPlayerModule::RequestChargeDash()
{
	if ( IsPlayingChargeAction() ) return false;

	if ( !CanExecuteChargedAction() )
	{
		CancelCharge( false );	// 閾値未満
		return false;
	}

	OnStartChargeDash();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule::RequestChargeAttack()
{
	if ( OwnerCharacter->IsAttacking() ) return false;

	// チャージダッシュ中は突進をキャンセルして即座にコンボチャージ攻撃へ派生させる
	if ( IsPlayingChargeDash() )
	{
		bChargeInputHeld = false;
		CancelCharge( false );
		OnStartChargeAttack();
		return true;
	}

	if ( IsPlayingChargeAction() ) return false;

	if ( !CanExecuteChargedAction() )
	{
		bChargeInputHeld = false;
		CancelCharge( false );	// 閾値未満
		OnStartLightAttack();
		return false;
	}

	OnStartChargeAttack();
	ClearChargeState();
	return true;
}

bool UChargeActionPlayerModule::RequestChargeJump()
{
	if ( IsPlayingChargeAction() ) return false;

	if ( !CanExecuteChargedAction() )
	{
		CancelCharge();	// 閾値未満
		return false;
	}

	OnStartChargeJump();
	ClearChargeState();
	return true;
}

void UChargeActionPlayerModule::OnCharacterHit( const FHitResult& Hit )
{
	if ( !IsPlayingChargeAction() ) return;

	// ジャンプ中は止めず、ダッシュと攻撃の突進中のみ対象とする
	if ( CurrentChargeActionType != EChargeActionType::Attack &&
		CurrentChargeActionType != EChargeActionType::Dash ) return;

	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	// 法線の上向き具合を見て、地面（斜面）との接触は壁衝突とみなさない
	UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement();
	if ( !MovementComp ) return;

	if ( Hit.Normal.Z >= MovementComp->GetWalkableFloorZ() )
	{
		return;
	}

	const FVector ForwardDir = OwnerCharacter->GetActorForwardVector();
	const float DotResult = FVector::DotProduct( ForwardDir, Hit.Normal );

	// 内積 -1.0＝真正面（180度）／0.0＝平行に擦る（90度）。
	// 浅い角度のかすりでは止めず、ある程度正面気味に当たった場合のみ停止させる
	const float StopThreshold = OwnerCharacter->PlayerParamData->ChargeMoveStopThreshold;
	if ( DotResult < StopThreshold )
	{
		OwnerCharacter->StopVelocity();
	}
}

float UChargeActionPlayerModule::GetCurrentChargeRatio() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( OwnerCharacter == nullptr || PlayerParams == nullptr )
	{
		return 0.0f;
	}
	return FMath::Clamp( CurrentChargeTimer.GetElapsed() / PlayerParams->MaxChargeTime, 0.0f, 1.0f );
}

bool UChargeActionPlayerModule::CanExecuteChargedAction() const
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams )
	{
		return false;
	}

	// コンボ 2 段目以降、または攻撃ステート・コンボ枠からの派生ならコンボ中とみなし、
	// 最低溜め時間を無視して即時発動を許可する
	const bool bIsDerivingFromAttack = OwnerCharacter->IsAttacking() ||
		OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo );

	if ( CurrentChargeComboIndex >= 2 || bIsDerivingFromAttack )
	{
		return true;
	}

	return CurrentChargeTimer.GetElapsed() >= PlayerParams->MinChargeTimeForDash;
}

void UChargeActionPlayerModule::ClearChargeState()
{
	bIsCharging = false;
	CurrentChargeTimer.Clear();

	DestroyChargeEffect();
}

void UChargeActionPlayerModule::ResetChargedActionState()
{
	ChargedActionLockTimer.Clear();
	CurrentChargeActionType = EChargeActionType::None;
	DestroyChargedDashEffect();
	RemainingGhostTrailDuration = 0.0f;
}

void UChargeActionPlayerModule::CancelCharge( bool bRestoreDash )
{
	if ( !OwnerCharacter ) return;

	StopCurrentChargeMontages();

	ResetChargedActionState();
	FrictionRecoveryTimer.Clear();

	// 開始前がダッシュならダッシュ状態へ復帰する
	if ( bWasDashingBeforeCharge && bRestoreDash )
	{
		ClearChargeState();
		bWasDashingBeforeCharge = false;

		OwnerCharacter->ForceStartDash();
		return;
	}

	ClearChargeState();

	OwnerCharacter->RefreshMovementParams();
}

void UChargeActionPlayerModule::OnEndAction()
{
	const EChargeActionType FinishedActionType = CurrentChargeActionType;
	ResetChargedActionState();

	if ( OwnerCharacter )
	{
		// チャージ継続中は自動でのダッシュ移行をスキップさせる
		if ( !bIsCharging && ( FinishedActionType == EChargeActionType::Dash ||
			FinishedActionType == EChargeActionType::Jump ) )
		{
			const FVector2D RawInput = OwnerCharacter->GetRawMovementInput();
			if ( RawInput.SizeSquared() > 0.01f )
			{
				// レバーが倒されていれば直接ダッシュへ
				ClearChargeState();
				CurrentChargeActionType = EChargeActionType::None;
				OwnerCharacter->ForceStartDash();
				return;
			}
		}

		if ( FinishedActionType == EChargeActionType::Dash && !bIsCharging )
		{
			OwnerCharacter->StopAnimMontage();
		}

		OwnerCharacter->RefreshMovementParams();
		if ( const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams() )
		{
			FrictionRecoveryTimer.Set( PlayerParams->ChargeActionFrictionRecoveryTime );
		}
		OwnerCharacter->UpdateFrictionRecovery( 0.0f );
	}

	if ( !bIsCharging ) ClearChargeState();
	CurrentChargeActionType = EChargeActionType::None;
}

FName UChargeActionPlayerModule::GetChargeStartAnimTag() const
{
	switch ( CurrentChargeComboIndex )
	{
	case 1: return PlayerAnimTags::CHARGE_01_ST;
	case 2: return PlayerAnimTags::CHARGE_02_ST;
	case 3: return PlayerAnimTags::CHARGE_03_ST;
	case 4: return PlayerAnimTags::CHARGE_04_ST;
	default: return PlayerAnimTags::CHARGE_01_ST;
	}
}

FName UChargeActionPlayerModule::GetChargeLoopAnimTag() const
{
	switch ( CurrentChargeComboIndex )
	{
	case 1: return PlayerAnimTags::CHARGE_01_LP_V1;
	case 2: return PlayerAnimTags::CHARGE_02_LP_V1;
	case 3: return PlayerAnimTags::CHARGE_03_LP_V1;
	case 4: return PlayerAnimTags::CHARGE_04_LP_V1;
	default: return PlayerAnimTags::CHARGE_01_LP_V1;
	}
}

void UChargeActionPlayerModule::OnStartChargeDash()
{
	CurrentChargeActionType = EChargeActionType::Dash;
	ExecuteChargePropulsion();
	PlayAnimMontage( PlayerAnimTags::DASH );

	if ( !ChargeDashCameraHandle.IsValid() )
	{
		if ( UExCameraSubsystem* CameraSubsystem = GetCameraSubsystem() )
		{
			ChargeDashCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( TEXT( "ChargeDash" ) );
			if ( const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams() )
			{
				ChargeDashCameraPopTimer.Set( PlayerParams->ChargeCameraPopDelay );
			}
		}
	}
}

void UChargeActionPlayerModule::OnStartChargeAttack()
{
	CurrentChargeActionType = EChargeActionType::Attack;
	ExecuteChargePropulsion();
	OwnerCharacter->RequestAttack( EPlayerAttackType::Charged );
}

void UChargeActionPlayerModule::OnStartChargeJump()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	OwnerCharacter->StopDash();

	CurrentChargeActionType = EChargeActionType::Jump;
	const float ChargeRate = CurrentChargeTimer.GetRate();

	// Z（高さ）
	const float MinJumpZ = OwnerCharacter->PlayerParamData->MinChargeJumpPower;
	const float MaxJumpZ = OwnerCharacter->PlayerParamData->MaxChargeJumpPower;
	const float FinalJumpZ = FMath::Lerp( MinJumpZ, MaxJumpZ, ChargeRate );

	// XY（前方へのダッシュ力）＝チャージ率で補間した基礎値 × スティック入力の大きさ
	const float MinDashPower = OwnerCharacter->PlayerParamData->MinChargeJumpMovePower;
	const float MaxDashPower = OwnerCharacter->PlayerParamData->MaxChargeJumpMovePower;
	const float BaseForwardPower = FMath::Lerp( MinDashPower, MaxDashPower, ChargeRate );

	const FVector2D MovementInput = OwnerCharacter->GetRawMovementInput();
	const float InputScale = FMath::Clamp( MovementInput.Size(), 0.0f, 1.0f );
	const float FinalForwardPower = BaseForwardPower * InputScale;

	const FRotator ActorRot = OwnerCharacter->GetActorRotation();
	const FRotator YawRotation( 0.0f, ActorRot.Yaw, 0.0f );
	const FVector ForwardDirection = YawRotation.Vector();

	const FVector LaunchVelocity = ( ForwardDirection * FinalForwardPower ) + FVector( 0.0f, 0.0f, FinalJumpZ );

	OwnerCharacter->LaunchCharacter( LaunchVelocity, false, true );

	const float FinalLockTime = FMath::Lerp(
		OwnerCharacter->PlayerParamData->MinChargeJumpMovementLockTime,
		OwnerCharacter->PlayerParamData->MaxChargeJumpMovementLockTime,
		ChargeRate
	);
	ChargedActionLockTimer.Set( FinalLockTime );
	StartGhostTrailSpawing( 100.0f );	// 残像を着地まで出し続けるため、大きめの値を入れておく

	SpawnChargedDashEffect();

	OwnerCharacter->RefreshMovementParams();
	OwnerCharacter->StopAnimMontage();
}

void UChargeActionPlayerModule::ExecuteChargePropulsion()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	const float ChargeRate = CurrentChargeTimer.GetRate();
	const FChargePropulsionSettings Settings = BuildChargePropulsionSettings( ChargeRate );
	const float FinalDashPower = FMath::Lerp( Settings.MinPower, Settings.MaxPower, ChargeRate );

	if ( class AController* Controller = OwnerCharacter->GetController() )
	{
		const FRotator ControlRot = OwnerCharacter->GetActorRotation();
		const FRotator YawRotation( 0.0f, ControlRot.Yaw, 0.0f );
		FVector DashDirection = YawRotation.Vector();

		// 付近に敵がいればその方向へ捻じ曲げ、見た目を合わせるため即座に振り向かせる
		DashDirection = GetHomingDirection( DashDirection );
		OwnerCharacter->SetActorRotation( DashDirection.Rotation() );

		// 坂道補正
		if ( UCharacterMovementComponent* MovementComp = OwnerCharacter->GetCharacterMovement() )
		{
			if ( MovementComp->IsMovingOnGround() )
			{
				const FVector FloorNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				DashDirection = FVector::VectorPlaneProject( DashDirection, FloorNormal ).GetSafeNormal();
			}
		}

		CachedChargeDashDirection = DashDirection;
		CachedChargeDashSpeed = Settings.Speed;

		OwnerCharacter->LaunchCharacter( DashDirection * FinalDashPower, true, true );

		ChargedActionLockTimer.Set( Settings.LockTime );
		StartGhostTrailSpawing( Settings.GhostTrailDuration );
		OwnerCharacter->RefreshMovementParams();
	}

	CurrentChargeTimer.Clear();
	SpawnChargedDashEffect();
}

bool UChargeActionPlayerModule::IsPlayingChargeAttackMontage() const
{
	if ( !OwnerCharacter || !OwnerCharacter->IsAttacking() )
	{
		return false;
	}

	UAnimMontage* CurrentMontage = OwnerCharacter->GetCurrentMontage();
	if ( CurrentMontage == nullptr )
	{
		return false;
	}

	return CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_01_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_02_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_03_ATK ) ||
		CurrentMontage == GetAnimMontage( PlayerAnimTags::CHARGE_04_ATK );
}

const UTidePlayerParamDataAsset* UChargeActionPlayerModule::GetPlayerParams() const
{
	return OwnerCharacter ? OwnerCharacter->PlayerParamData : nullptr;
}

UCharacterMovementComponent* UChargeActionPlayerModule::GetCharacterMovement() const
{
	return OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
}

UExCameraSubsystem* UChargeActionPlayerModule::GetCameraSubsystem() const
{
	if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter ? OwnerCharacter->GetController() : nullptr ) )
	{
		if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
		{
			return LocalPlayer->GetSubsystem<UExCameraSubsystem>();
		}
	}

	return nullptr;
}

FVector UChargeActionPlayerModule::GetActorYawForwardDirection() const
{
	if ( !OwnerCharacter )
	{
		return FVector::ForwardVector;
	}

	const FRotator ActorRot = OwnerCharacter->GetActorRotation();
	const FRotator YawRot( 0.0f, ActorRot.Yaw, 0.0f );
	return YawRot.Vector();
}

FVector UChargeActionPlayerModule::ProjectDirectionToGround( const FVector& InDirection ) const
{
	FVector ProjectedDirection = InDirection;

	if ( UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		if ( MovementComp->IsMovingOnGround() )
		{
			const FVector FloorNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
			ProjectedDirection = FVector::VectorPlaneProject( ProjectedDirection, FloorNormal ).GetSafeNormal();
		}
	}

	return ProjectedDirection;
}

bool UChargeActionPlayerModule::ShouldContinueChargeCombo() const
{
	return OwnerCharacter && ( OwnerCharacter->HasStateTag( TAG_State_Player_CanCombo ) || IsPlayingChargeAttackMontage() );
}

void UChargeActionPlayerModule::UpdateChargeComboIndex()
{
	if ( ShouldContinueChargeCombo() )
	{
		if ( CurrentChargeComboIndex < MaxChargeComboCount )
		{
			CurrentChargeComboIndex++;
		}
		return;
	}

	CurrentChargeComboIndex = 1;
}

void UChargeActionPlayerModule::InterruptCurrentChargeAction()
{
	if ( IsPlayingChargeAction() )
	{
		ResetChargedActionState();
	}
}

void UChargeActionPlayerModule::PrepareOwnerForCharge()
{
	OwnerCharacter->CancelDodge();

	if ( OwnerCharacter->IsAttacking() )
	{
		OwnerCharacter->CancelAttack();
	}

	bWasDashingBeforeCharge = OwnerCharacter->IsDashing();

	if ( bWasDashingBeforeCharge )
	{
		OwnerCharacter->StopDash();
	}
}

void UChargeActionPlayerModule::StartChargeState()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !PlayerParams )
	{
		return;
	}

	bIsCharging = true;
	bHasReachedMaxCharge = false;
	CurrentChargeTimer.Set( PlayerParams->MaxChargeTime );
	FrictionRecoveryTimer.Clear();
}

void UChargeActionPlayerModule::PlayChargeStartMontage()
{
	OwnerCharacter->StopAnimMontage();
	PlayAnimMontage( GetChargeStartAnimTag() );
}

void UChargeActionPlayerModule::ApplyChargeStartMovement()
{
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	OwnerCharacter->RefreshMovementParams();

	if ( !PlayerParams || PlayerParams->ChargeBeginMovePower <= 0.0f )
	{
		return;
	}

	const FVector ForwardDir = ProjectDirectionToGround( GetActorYawForwardDirection() );

	OwnerCharacter->LaunchCharacter( ForwardDir * PlayerParams->ChargeBeginMovePower, false, false );
}

void UChargeActionPlayerModule::StopCurrentChargeMontages()
{
	UAnimMontage* StartMontage = GetAnimMontage( GetChargeStartAnimTag() );
	UAnimMontage* LoopMontage = GetAnimMontage( GetChargeLoopAnimTag() );

	if ( StartMontage && OwnerCharacter->GetCurrentMontage() == StartMontage )
	{
		OwnerCharacter->StopAnimMontage( StartMontage );
	}

	if ( LoopMontage && OwnerCharacter->GetCurrentMontage() == LoopMontage )
	{
		OwnerCharacter->StopAnimMontage( LoopMontage );
	}
}

UChargeActionPlayerModule::FChargePropulsionSettings UChargeActionPlayerModule::BuildChargePropulsionSettings( float ChargeRate ) const
{
	FChargePropulsionSettings Settings;
	const UTidePlayerParamDataAsset* PlayerParams = GetPlayerParams();
	if ( !OwnerCharacter || !PlayerParams )
	{
		return Settings;
	}

	switch ( CurrentChargeActionType )
	{
	case EChargeActionType::Dash:
		Settings.Speed = FMath::Lerp(
			PlayerParams->MinChargeDashMoveSpeed,
			PlayerParams->MaxChargeDashMoveSpeed,
			ChargeRate
		);
		Settings.LockTime = FMath::Lerp(
			PlayerParams->MinChargeDashTime,
			PlayerParams->MaxChargeDashTime,
			ChargeRate
		);
		Settings.GhostTrailDuration = Settings.LockTime;
		break;

	case EChargeActionType::Attack:
		Settings.MinPower = PlayerParams->MinChargeAttackMovePower;
		Settings.MaxPower = PlayerParams->MaxChargeAttackMovePower;
		Settings.LockTime = FMath::Lerp(
			PlayerParams->MinChargeAttackMovementLockTime,
			PlayerParams->MaxChargeAttackMovementLockTime,
			ChargeRate
		);
		Settings.GhostTrailDuration = 0.275f;
		break;

	default:
		break;
	}

	return Settings;
}

void UChargeActionPlayerModule::OnStartLightAttack()
{
	if ( !OwnerCharacter ) return;
	OwnerCharacter->RequestAttack( EPlayerAttackType::Light );
}

FVector UChargeActionPlayerModule::GetHomingDirection( const FVector& InDefaultDir, float InDebugDuration ) const
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !OwnerCharacter->PlayerParamData->bEnableChargeAttackHoming )
	{
		return InDefaultDir;
	}

	const int32 GearIdx = 1;

	// ギアごとの吸着距離・角度（設定漏れ防止のフェイルセーフ付き）
	static constexpr float FallbackMaxDist = 2000.0f;
	float MaxDist = FallbackMaxDist;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear.IsValidIndex( GearIdx ) )
	{
		MaxDist = OwnerCharacter->PlayerParamData->ChargeAttackHomingDistanceForGear[GearIdx];
	}

	static constexpr float FallbackAngle = 10.0f;
	float Angle = FallbackAngle;
	if ( OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear.IsValidIndex( GearIdx ) )
	{
		Angle = OwnerCharacter->PlayerParamData->ChargeAttackHomingAngleForGear[GearIdx];
	}

	const float HalfAngleRad = FMath::DegreesToRadians( Angle * 0.5f );
	const FVector MyLoc = OwnerCharacter->GetActorLocation();

	ULockOnTargetComponent* LockedTargetComp = nullptr;
	if ( OwnerCharacter->IsLockOnActive() && OwnerCharacter->GetLockOnComponent() != nullptr )
	{
		LockedTargetComp = OwnerCharacter->GetLockOnComponent()->GetTarget();
	}

	FVector ResultDir = InDefaultDir;
	ULockOnTargetComponent* FinalTargetComp = nullptr;

	// ロックオン対象を優先し、無ければ扇形サーチで最寄りを拾う
	if ( LockedTargetComp != nullptr )
	{
		ResultDir = ( LockedTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		FinalTargetComp = LockedTargetComp;
	}
	else
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor( OwnerCharacter );

		OwnerCharacter->GetWorld()->OverlapMultiByChannel(
			Overlaps, MyLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere( MaxDist ), Params
		);

		float ClosestDistSq = FMath::Square( MaxDist );

		for ( const FOverlapResult& Result : Overlaps )
		{
			AActor* PotentialActor = Result.GetActor();
			if ( !PotentialActor ) continue;
			if ( !IsTargetableActor( PotentialActor ) ) continue;

			TArray<ULockOnTargetComponent*> TargetComps;
			PotentialActor->GetComponents<ULockOnTargetComponent>( TargetComps );

			for ( ULockOnTargetComponent* PotentialTargetComp : TargetComps )
			{
				if ( !PotentialTargetComp || !PotentialTargetComp->bIsTargetable ) continue;

				FVector DirToTarget = ( PotentialTargetComp->GetTargetLocation() - MyLoc );
				const float DistSq = DirToTarget.SizeSquared();
				DirToTarget.Normalize();

				const float Dot = FVector::DotProduct( InDefaultDir, DirToTarget );
				if ( Dot >= FMath::Cos( HalfAngleRad ) )
				{
					if ( DistSq < ClosestDistSq )
					{
						ClosestDistSq = DistSq;
						FinalTargetComp = PotentialTargetComp;
					}
				}
			}
		}

		if ( FinalTargetComp )
		{
			ResultDir = ( FinalTargetComp->GetTargetLocation() - MyLoc ).GetSafeNormal2D();
		}
	}

	return ResultDir;
}


bool UChargeActionPlayerModule::IsTargetableActor( const AActor* InActor ) const
{
	if ( InActor == nullptr || InActor == OwnerCharacter )
	{
		return false;
	}

	const ULockOnTargetComponent* TargetComp = InActor->FindComponentByClass<ULockOnTargetComponent>();
	if ( TargetComp != nullptr && TargetComp->bIsTargetable )
	{
		return true;
	}

	return false;
}

void UChargeActionPlayerModule::SpawnChargeEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGING ) )
	{
		SpawnedChargeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
	}
}

void UChargeActionPlayerModule::SpawnChargedDashEffect()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;
	if ( UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::CHARGED_DASH ) )
	{
		SpawnedChargedDashEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
			EffectSys,
			OwnerCharacter->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
		FLinearColor NewColor( 1.0f, 0.0f, 0.0f, 1.0f );
		SpawnedChargedDashEffect->SetColorParameter( "Color", NewColor );
	}
}

void UChargeActionPlayerModule::DestroyChargeEffect()
{
	if ( SpawnedChargeEffect )
	{
		SpawnedChargeEffect->DestroyComponent();
		SpawnedChargeEffect = nullptr;
	}
}

void UChargeActionPlayerModule::DestroyChargedDashEffect()
{
	if ( SpawnedChargedDashEffect )
	{
		SpawnedChargedDashEffect->DestroyComponent();
		SpawnedChargedDashEffect = nullptr;
	}
}

void UChargeActionPlayerModule::StartGhostTrailSpawing( float Duration )
{
	RemainingGhostTrailDuration = Duration;
	SpawnGhostTrail();	// 初回分

	if ( OwnerCharacter && OwnerCharacter->PlayerParamData )
	{
		GhostTrailSpawnTimer.Set( OwnerCharacter->PlayerParamData->GhostTrailSpawnInterval );
	}
}

void UChargeActionPlayerModule::UpdateGhostTrails( float DeltaTime )
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData ) return;

	if ( RemainingGhostTrailDuration > 0.0f )
	{
		RemainingGhostTrailDuration -= DeltaTime;
		GhostTrailSpawnTimer.Update( DeltaTime );

		if ( GhostTrailSpawnTimer.IsFinish() )
		{
			SpawnGhostTrail();
			GhostTrailSpawnTimer.Set( OwnerCharacter->PlayerParamData->GhostTrailSpawnInterval );
		}
	}

	// 末尾からループして安全に削除する
	for ( int32 i = ActiveGhostTrails.Num() - 1; i >= 0; --i )
	{
		FGhostTrailData& Trail = ActiveGhostTrails[i];
		Trail.ElapsedTime += DeltaTime;

		if ( Trail.ElapsedTime >= Trail.Lifespan )
		{
			if ( IsValid( Trail.MeshComponent ) )
			{
				Trail.MeshComponent->DestroyComponent();
			}
			ActiveGhostTrails.RemoveAt( i );
			continue;
		}

		// フェードアウト（1.0 → 0.0）
		const float FadeAlpha = 1.0f - ( Trail.ElapsedTime / Trail.Lifespan );
		for ( UMaterialInstanceDynamic* MID : Trail.MIDs )
		{
			if ( IsValid( MID ) )
			{
				MID->SetScalarParameterValue( FName( "FadeAmount" ), FadeAlpha );
			}
		}
	}
}

void UChargeActionPlayerModule::SpawnGhostTrail()
{
	if ( !OwnerCharacter || !OwnerCharacter->PlayerParamData || !OwnerCharacter->PlayerParamData->ChargeActionGhostTrailMaterial ) return;

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	if ( !CharacterMesh ) return;

	UPoseableMeshComponent* GhostMesh = NewObject<UPoseableMeshComponent>( OwnerCharacter );
	if ( !GhostMesh ) return;

	GhostMesh->RegisterComponent();
	GhostMesh->SetWorldLocationAndRotation( CharacterMesh->GetComponentLocation(), CharacterMesh->GetComponentRotation() );
	GhostMesh->SetSkinnedAssetAndUpdate( CharacterMesh->GetSkeletalMeshAsset() );
	GhostMesh->CopyPoseFromSkeletalComponent( CharacterMesh );
	GhostMesh->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	GhostMesh->CastShadow = false;

	FGhostTrailData NewTrailData;
	NewTrailData.MeshComponent = GhostMesh;
	NewTrailData.Lifespan = OwnerCharacter->PlayerParamData->GhostTrailLifespan;

	// フェード制御のため MID へ差し替えて保持する
	const int32 NumMaterials = GhostMesh->GetNumMaterials();
	for ( int32 i = 0; i < NumMaterials; ++i )
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create( OwnerCharacter->PlayerParamData->ChargeActionGhostTrailMaterial, GhostMesh );
		if ( MID )
		{
			MID->SetScalarParameterValue( FName( "FadeAmount" ), 1.0f );
			GhostMesh->SetMaterial( i, MID );
			NewTrailData.MIDs.Add( MID );
		}
	}

	UWorld* World = OwnerCharacter->GetWorld();
	if ( World )
	{
		GhostMesh->RegisterComponentWithWorld( World );
	}

	ActiveGhostTrails.Add( NewTrailData );
}

FName UChargeActionPlayerModule::GetChargeAttackAnimTag() const
{
	switch ( CurrentChargeComboIndex )
	{
	case 1: return PlayerAnimTags::CHARGE_01_ATK;
	case 2: return PlayerAnimTags::CHARGE_02_ATK;
	case 3: return PlayerAnimTags::CHARGE_03_ATK;
	case 4: return PlayerAnimTags::CHARGE_04_ATK;
	}
	return PlayerAnimTags::CHARGE_01_ATK;
}

void UChargeActionPlayerModule::DrawChargeGaugeUI( float InCurrentTime, float InMaxTime, float InMinThreshold, const FVector& InPlayerLocation, APlayerController* InPlayerController )
{
	if ( InPlayerController == nullptr || InMaxTime <= 0.0f )
	{
		return;
	}

	FVector2D ScreenPos;
	if ( !InPlayerController->ProjectWorldLocationToScreen( InPlayerLocation, ScreenPos ) )
	{
		return;	// カメラの背後（画面外）
	}

	// プレイヤーの右側へ少しずらす
	ScreenPos.X += 80.0f;
	ScreenPos.Y -= 50.0f;

	// 毎フレーム追従させるため Cond 指定なしで位置を強制上書きする
	ImGui::SetNextWindowPos( ImVec2( ScreenPos.X, ScreenPos.Y ) );
	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.1f, 0.1f, 0.1f, 0.5f ) );
	ImGui::Begin( "Charge Action", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings );

	ImU32 GaugeColor;
	if ( InCurrentTime >= InMaxTime )
	{
		GaugeColor = IM_COL32( 255, 128, 0, 255 );	// MAX
	}
	else if ( InCurrentTime >= InMinThreshold )
	{
		GaugeColor = IM_COL32( 50, 200, 255, 255 );	// ダッシュ可能
	}
	else
	{
		GaugeColor = IM_COL32( 100, 100, 100, 255 );	// 閾値未満（不発）
	}

	const float ChargeRatio = FMath::Clamp( InCurrentTime / InMaxTime, 0.0f, 1.0f );
	const float ThresholdRatio = FMath::Clamp( InMinThreshold / InMaxTime, 0.0f, 1.0f );

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Pos = ImGui::GetCursorScreenPos();
	ImVec2 Size( 20.0f, 100.0f );

	DrawList->AddRectFilled( Pos, ImVec2( Pos.x + Size.x, Pos.y + Size.y ), IM_COL32( 40, 40, 40, 255 ) );

	// 閾値の目安となる横線
	float ThresholdY = Pos.y + Size.y * ( 1.0f - ThresholdRatio );
	DrawList->AddLine( ImVec2( Pos.x, ThresholdY ), ImVec2( Pos.x + Size.x, ThresholdY ), IM_COL32( 255, 255, 255, 180 ), 1.0f );

	// 現在のチャージ量を下から上へ
	float FillHeight = Size.y * ChargeRatio;
	DrawList->AddRectFilled( ImVec2( Pos.x, Pos.y + Size.y - FillHeight ), ImVec2( Pos.x + Size.x, Pos.y + Size.y ), GaugeColor );

	ImGui::Dummy( Size );	// レイアウトカーソルをゲージの高さ分だけ進める
	ImGui::Text( "%.1f / %.1f", InCurrentTime, InMaxTime );

	ImGui::End();
	ImGui::PopStyleColor();
}
