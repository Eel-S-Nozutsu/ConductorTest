// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "TidePlayerCharacter.h"

#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/App.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/ChargeActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/ChargeActionPlayerModule_V2.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/DodgeActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/DashActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/AttackActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/LockOnPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/HitReactionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/JumpActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GlideActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/FallActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassivePlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/FinisherPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/LaunchActionLockPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/BoostDashPlayerModule.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodBirdPlayerModule.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Player/PlayerAnimInstance.h"
#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Components/Input/InputBufferComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Components/Player/FallRecoveryComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/State/StateTagComponent.h"
#include "PRJ_TIDE_P0/Components/Player/RestartComponent.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

// Debug
#include "Engine/DamageEvents.h"
#if !UE_BUILD_SHIPPING
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/PlayerWindow.h"
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/PlayerHudPlaceholder.h"
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/GodActionHudPlaceholder.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"
#endif

ATidePlayerCharacter::ATidePlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize( 42.f, 96.0f );

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->RotationRate = FRotator( 0.0f, 500.0f, 0.0f );

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	InputBufferComponent = CreateDefaultSubobject<UInputBufferComponent>( TEXT( "InputBufferComponent" ) );
	LockOnComponent = CreateDefaultSubobject<ULockOnComponent>( TEXT( "LockOnComponent" ) );
	FallRecoveryComponent = CreateDefaultSubobject<UFallRecoveryComponent>( TEXT( "FallRecoveryComponent" ) );
	RestartComponent = CreateDefaultSubobject<URestartComponent>( TEXT( "RestartComponent" ) );

	if ( UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
	}
}

void ATidePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	BasicPermissionTags.Add( TAG_State_Player_CanMove );
	BasicPermissionTags.Add( TAG_State_Player_CanDash );
	BasicPermissionTags.Add( TAG_State_Player_CanAttack );
	BasicPermissionTags.Add( TAG_State_Player_CanCharge );
	if ( UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance() )
	{
		AnimInstance->OnMontageEnded.AddDynamic( this, &ATidePlayerCharacter::OnMontageEnded );
		AnimInstance->OnMontageBlendingOut.AddDynamic( this, &ATidePlayerCharacter::OnMontageBlendingOut );
	}
	if ( auto* StatusComp = GetStatusComponent() )
	{
		StatusComp->OnDeath.AddDynamic( this, &ATidePlayerCharacter::OnStatusDeath );
		StatusComp->OnRevive.AddDynamic( this, &ATidePlayerCharacter::OnStatusRevive );
	}
	ApplyNormalMovementParams();

	// 既定 500cm のままだと、めり込んだ瞬間に最大 5m 打ち上げられる
	if ( PlayerParamData && PlayerParamData->MaxDepenetrationWithGeometry > 0.0f )
	{
		if ( auto* Movement = GetCharacterMovement() )
		{
			Movement->MaxDepenetrationWithGeometry = PlayerParamData->MaxDepenetrationWithGeometry;
		}
	}

	SetupModules();
}

void ATidePlayerCharacter::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	if ( UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr )
	{
		AnimInstance->OnMontageEnded.RemoveDynamic( this, &ATidePlayerCharacter::OnMontageEnded );
		AnimInstance->OnMontageBlendingOut.RemoveDynamic( this, &ATidePlayerCharacter::OnMontageBlendingOut );
	}
	if ( auto* StatusComp = GetStatusComponent() )
	{
		StatusComp->OnDeath.RemoveDynamic( this, &ATidePlayerCharacter::OnStatusDeath );
		StatusComp->OnRevive.RemoveDynamic( this, &ATidePlayerCharacter::OnStatusRevive );
	}

	Super::EndPlay( EndPlayReason );
}

void ATidePlayerCharacter::SetupModules()
{
	CachedChargeModule = NewObject<UChargeActionPlayerModule>( this );
	Modules.Add( CachedChargeModule );
	CachedChargeModuleV2 = NewObject<UChargeActionPlayerModule_V2>( this );
	Modules.Add( CachedChargeModuleV2 );
	CachedDodgeModule = NewObject<UDodgeActionPlayerModule>( this );
	Modules.Add( CachedDodgeModule );
	CachedDashModule = NewObject<UDashActionPlayerModule>( this );
	Modules.Add( CachedDashModule );
	CachedAttackModule = NewObject<UAttackActionPlayerModule>( this );
	Modules.Add( CachedAttackModule );
	CachedLockOnModule = NewObject<ULockOnPlayerModule>( this );
	Modules.Add( CachedLockOnModule );
	CachedHitReactionModule = NewObject<UHitReactionPlayerModule>( this );
	Modules.Add( CachedHitReactionModule );
	// 滑空はジャンプより先に Tick する（同フレームで JumpModule のモーション制御を抑止するため）
	CachedGlideModule = NewObject<UGlideActionPlayerModule>( this );
	Modules.Add( CachedGlideModule );
	CachedJumpModule = NewObject<UJumpActionPlayerModule>( this );
	Modules.Add( CachedJumpModule );
	CachedFallModule = NewObject<UFallActionPlayerModule>( this );
	Modules.Add( CachedFallModule );
	CachedGodActionModule = NewObject<UGodActionPlayerModule>( this );
	Modules.Add( CachedGodActionModule );
	CachedSlidePassiveModule = NewObject<USlidePassivePlayerModule>( this );
	Modules.Add( CachedSlidePassiveModule );
	CachedFinisherModule = NewObject<UFinisherPlayerModule>( this );
	Modules.Add( CachedFinisherModule );
	CachedLaunchLockModule = NewObject<ULaunchActionLockPlayerModule>( this );
	Modules.Add( CachedLaunchLockModule );
	CachedBoostDashModule = NewObject<UBoostDashPlayerModule>( this );
	Modules.Add( CachedBoostDashModule );
	CachedGodBirdModule = NewObject<UGodBirdPlayerModule>( this );
	Modules.Add( CachedGodBirdModule );

	for ( auto& Module : Modules )
	{
		if ( Module != nullptr )
		{
			Module->Initialize( this );
		}
	}
}

bool ATidePlayerCharacter::IsUsingChargeV2() const
{
	return PlayerParamData && PlayerParamData->bUseChargeV2;
}

void ATidePlayerCharacter::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	BeginMovementSpikeFrame( DeltaTime );

	// 各モジュールの Slope 系がこの値を参照するので先に均す
	UpdateSmoothedFloorNormal( DeltaTime );

	for ( const auto& Module : Modules )
	{
		if ( Module )
		{
			Module->OnModuleUpdate( DeltaTime );
		}
	}

	if ( bMovementSpikeFrameValid )
	{
		MovementSpikeVelAfterModules = GetVelocity();
	}

	// モジュール側ではなくここで呼ぶ。モジュールが早期 return しても必ず走らせないと、
	// 面沿いモードの重力方向と姿勢が戻らないまま取り残される
	UpdateSurfaceRide( DeltaTime );

	// 上方向が決まった後・入力処理より前に 1 回だけ
	UpdateSurfaceRideInputBasis( DeltaTime );

	MarkMovementSpikeMidTick();

	// 一閃の斬りかかり移動を妨げないため、発動専有中は押し出さない（構え選択中は通常どおり）
	if ( !IsGodActionExecuting() )
	{
		UpdatePushPawn( DeltaTime );
	}

	UpdateHorizontalWindPush( DeltaTime );

	EndMovementSpikeFrame( DeltaTime );

	UpdateUI();
#if !UE_BUILD_SHIPPING
	DrawDebugCoordinate();
#endif
}

void ATidePlayerCharacter::UnPossessed()
{
	Super::UnPossessed();
}

void ATidePlayerCharacter::RefreshMovementParams()
{
	if ( !PlayerParamData ) return;

	// ブーストダッシュは最優先（他状態より上限速度・低摩擦を通す）
	if ( IsBoostDashing() )
	{
		ApplyBoostDashMovementParams();
		UE_LOG( LogTemp, Verbose, TEXT( "[Player] ブーストダッシュパラメータ適応。" ) );
		return;
	}

	if ( IsDashing() )
	{
		ApplyDashActionMovementParams();
		UE_LOG( LogTemp, Verbose, TEXT( "[Player] ダッシュパラメータ適応。" ) );
		return;
	}

	if ( IsDodging() )
	{
		ApplyDodgeActionMovementParams();
		UE_LOG( LogTemp, Verbose, TEXT( "[Player] 回避パラメータ適応。" ) );
		return;
	}

	if ( IsPlayingChargeAction() )
	{
		if ( IsPlayingChargeJump() )
		{
			ApplyChargeJumpMovementParams();
			UE_LOG( LogTemp, Verbose, TEXT( "[Player] チャージジャンプパラメータ適応。" ) );
		}
		else if ( IsPlayingChargeDash() )
		{
			ApplyChargeDashMovementParams();
			UE_LOG( LogTemp, Verbose, TEXT( "[Player] チャージダッシュパラメータ適応。" ) );
		}
		else if ( IsPlayingChargeAttack() )
		{
			ApplyChargeAttackMovementParams();
			UE_LOG( LogTemp, Verbose, TEXT( "[Player] チャージアタックパラメータ適応。" ) );
		}
		return;
	}

	if ( IsCharging() )
	{
		ApplyChargingMovementParams();
		UE_LOG( LogTemp, Verbose, TEXT( "[Player] チャージ中パラメータ適応。" ) );
		return;
	}

	if ( IsLockOnActive() )
	{
		ApplyLockOnMovementParams();
		UE_LOG( LogTemp, Verbose, TEXT( "[Player] ロックオン中パラメータ適応。" ) );
		return;
	}

	ApplyNormalMovementParams();
	UE_LOG( LogTemp, Verbose, TEXT( "[Player] 通常パラメータ適応。" ) );
}

void ATidePlayerCharacter::ApplyNormalMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->MaxWalkSpeed = PlayerParamData->MaxWalkSpeed;
	// UpdateChargingMovementParams が上げた MinAnalogWalkSpeed の唯一の復元点
	// （死亡復帰・被弾・チャージ終了いずれも最終的にここを通る）
	Movement->MinAnalogWalkSpeed = PlayerParamData->MinAnalogWalkSpeed;

	Movement->JumpZVelocity = PlayerParamData->JumpZVelocity;
	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->AirControl = PlayerParamData->AirControl;
	Movement->AirControlBoostMultiplier = PlayerParamData->AirControlBoostMultiplier;
	Movement->AirControlBoostVelocityThreshold = PlayerParamData->AirControlBoostVelocityThreshold;

	const float TargetYaw = Movement->IsFalling() ? PlayerParamData->FallingRotationRateYaw : PlayerParamData->RotationRateYaw;
	Movement->RotationRate = FRotator( 0.0f, TargetYaw, 0.0f );

	Movement->GroundFriction = PlayerParamData->GroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->BrakingDecelerationWalking;
	Movement->BrakingDecelerationFalling = PlayerParamData->BrakingDecelerationFalling;

	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );
}

bool ATidePlayerCharacter::WantsSurfaceRideWalkableRaise() const
{
	if ( !PlayerParamData || !PlayerParamData->bEnableSurfaceRide ) return false;

	// ライド中は必ず維持する（急斜面の上で下げると足元が歩行不可になって落とされる）。
	// 下の入場条件は UpdateSurfaceRide 側と揃えること（ズレると着地判定が出ない）
	if ( bIsSurfaceRiding ) return true;

	const bool bChargeState = IsCharging() || IsPlayingChargeDash() || IsSurfaceRideChargeHopEntryAllowed();
	const bool bZoneAllowed = !PlayerParamData->bSurfaceRideRequireZone || IsInSurfaceRideZone();
	return bChargeState && bZoneAllowed;
}

bool ATidePlayerCharacter::IsSurfaceRideChargeHopEntryAllowed() const
{
	// 幅跳びは着地 ED の間も Jump 種別のままで、その後 ResumeChargeDashAfterLanding がダッシュへ戻す。
	// チャージ状態が途切れないので、入場さえ許せば維持は既存のダッシュ経路が担う
	return PlayerParamData && PlayerParamData->bSurfaceRideAllowChargeHopEntry && IsPlayingChargeHopJump();
}

void ATidePlayerCharacter::ApplyWalkableFloorAngle( float BaseAngle )
{
	auto* Movement = GetCharacterMovement();
	if ( !Movement || !PlayerParamData ) return;

	// 登坂角度の決定はここ 1 本に集約する。直接 SetWalkableFloorAngle すると、面沿いが上げた角度を
	// CMC のコールバック（OnLanded／OnMovementModeChanged）経由の RefreshMovementParams が引き下げ、
	// 壁の上で歩行⇔落下のフリップを起こす
	const float Target = WantsSurfaceRideWalkableRaise()
		? GetSlopeParamForGear( PlayerParamData->SurfaceRideWalkableFloorAngleForGear )
		: BaseAngle;

	if ( !FMath::IsNearlyEqual( Movement->GetWalkableFloorAngle(), Target ) )
	{
		Movement->SetWalkableFloorAngle( Target );
	}
}

void ATidePlayerCharacter::ApplyChargingMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	const float BaseWalkSpeed = PlayerParamData->MaxWalkSpeed;
	Movement->MaxWalkSpeed = BaseWalkSpeed * PlayerParamData->ChargingWalkSpeedRate;
	Movement->GravityScale = PlayerParamData->ChargingGravityScale;
	Movement->AirControl = PlayerParamData->ChargingAirControl;

	const float TargetYaw = Movement->IsFalling() ? PlayerParamData->ChargingFallingRotationRateYaw : PlayerParamData->ChargingRotationRateYaw;
	Movement->RotationRate = FRotator( 0.0f, TargetYaw, 0.0f );

	Movement->GroundFriction = PlayerParamData->ChargingGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->ChargingBrakingDecelerationWalking;
	Movement->BrakingDecelerationFalling = PlayerParamData->ChargingBrakingDecelerationFalling;

	// チャージ専用の登坂角度は、スライドだけ基準が低いと境界で落下・加速のフリップを起こすため廃止した
	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );
}

void ATidePlayerCharacter::ApplyChargeDashMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->AirControl = PlayerParamData->AirControl;

	Movement->GroundFriction = PlayerParamData->ChargeDashGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->ChargeDashBrakingDecelerationWalking;
	Movement->BrakingDecelerationFalling = PlayerParamData->ChargeDashBrakingDecelerationFalling;

	Movement->MaxWalkSpeed = PlayerParamData->MaxWalkSpeed;

	Movement->RotationRate = FRotator( 0.0f, GetChargeDashRotationRateYaw(), 0.0f );

	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );
}

void ATidePlayerCharacter::ApplyChargeAttackMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->AirControl = PlayerParamData->AirControl;

	Movement->GroundFriction = PlayerParamData->ChargeAttackGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->ChargeAttackBrakingDecelerationWalking;
	Movement->BrakingDecelerationFalling = PlayerParamData->ChargeAttackBrakingDecelerationFalling;

	Movement->MaxWalkSpeed = PlayerParamData->MaxWalkSpeed;

	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );
}

void ATidePlayerCharacter::ApplyChargeJumpMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;

	Movement->GroundFriction = PlayerParamData->ChargeJumpGroundFriction;
	Movement->BrakingDecelerationFalling = PlayerParamData->ChargeJumpBrakingDecelerationFalling;

	Movement->MaxWalkSpeed = PlayerParamData->MaxWalkSpeed * PlayerParamData->ChargeJumpWalkSpeedRate;
	Movement->AirControl = PlayerParamData->ChargeJumpAirControl;
	Movement->AirControlBoostMultiplier = PlayerParamData->ChargeJumpAirControlBoostMultiplier;
	Movement->AirControlBoostVelocityThreshold = PlayerParamData->ChargeJumpAirControlBoostVelocityThreshold;

	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );
}

void ATidePlayerCharacter::ApplyDodgeActionMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->GroundFriction = PlayerParamData->DodgeStepGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->DodgeStepBrakingDeceleration;
	Movement->BrakingDecelerationFalling = PlayerParamData->DodgeStepBrakingDecelerationFalling;
}

void ATidePlayerCharacter::ApplyDashActionMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->MaxWalkSpeed = PlayerParamData->DashWalkSpeed;
	float TargetYaw = PlayerParamData->DashRotationYaw;
	if ( CachedDashModule && CachedDashModule->IsInitialTurnBoostActive() )
	{
		TargetYaw = PlayerParamData->DashInitialRotationYaw;
	}
	Movement->RotationRate = FRotator( 0.0f, TargetYaw, 0.0f );
	Movement->GroundFriction = PlayerParamData->DashGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->DashBrakingDeceleration;
	Movement->BrakingDecelerationFalling = PlayerParamData->DashBrakingDecelerationFalling;
}

void ATidePlayerCharacter::ApplyLockOnMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->MaxWalkSpeed = PlayerParamData->LockOnMaxWalkSpeed;
	// ロックオン中に被弾した場合、チャージ中に上がった MinAnalogWalkSpeed がここで戻らず残るため復元する
	Movement->MinAnalogWalkSpeed = PlayerParamData->MinAnalogWalkSpeed;
	Movement->RotationRate = FRotator( 0.0f, PlayerParamData->LockOnRotationRateYaw, 0.0f );

	// todo: 攻撃ノックバック後の摩擦戻し処理が必要（今は戻さないことで滑らせている）
}

void ATidePlayerCharacter::ApplyBoostDashMovementParams()
{
	auto* Movement = GetCharacterMovement();
	if ( !PlayerParamData || !Movement ) return;

	Movement->GravityScale = PlayerParamData->GravityScale;
	Movement->AirControl = PlayerParamData->AirControl;

	// 上限速度はギミック側の指定値。取れなければ通常速度へフォールバック
	float BoostSpeed = PlayerParamData->MaxWalkSpeed;
	if ( CachedBoostDashModule && CachedBoostDashModule->GetBoostMaxSpeed() > 0.0f )
	{
		BoostSpeed = CachedBoostDashModule->GetBoostMaxSpeed();
	}
	Movement->MaxWalkSpeed = BoostSpeed;

	// 接地状態が変わったときの再適用は UBoostDashPlayerModule 側が RefreshMovementParams で行う
	const float BoostRotationRateYaw = Movement->IsFalling()
		? PlayerParamData->BoostDashRotationRateYawAir
		: PlayerParamData->BoostDashRotationRateYawGround;
	Movement->RotationRate = FRotator( 0.0f, BoostRotationRateYaw, 0.0f );
	Movement->GroundFriction = PlayerParamData->BoostDashGroundFriction;
	Movement->BrakingDecelerationWalking = PlayerParamData->BoostDashBrakingDecelerationWalking;
	Movement->BrakingDecelerationFalling = PlayerParamData->BoostDashBrakingDecelerationFalling;
}

void ATidePlayerCharacter::RemoveBasicPermissionTags()
{
	if ( UStateTagComponent* TagComp = FindComponentByClass<UStateTagComponent>() )
	{
		for ( const FGameplayTag& Tag : BasicPermissionTags )
		{
			TagComp->RemoveStateTag( Tag );
		}
	}
}

void ATidePlayerCharacter::RequestMove( float Right, float Forward )
{
	if ( !GetController() ) return;
	if ( IsDead() ) return;
	// 打ち上げ中は全アクションを Disable で封じるが、空中の方向制御だけは残す
	if ( HasStateTag( TAG_State_Common_Disable ) && !IsLaunchActionLocked() ) return;
	if ( IsChargeDashEndMovementLocked() ) return;
	if ( IsGodArtSelecting()
		&& PlayerParamData && !PlayerParamData->bGodArtStanceAllowMovement ) return;

	CachedMovementInput = FVector2D( Right, Forward );

	// 滑空中の移動・回頭は UGlideActionPlayerModule に一任する（入力の確保だけ済ませて抜ける）
	if ( IsInGlideSession() ) return;

	// 吹き飛びキャンセルの緊急ジャンプ中と被弾リアクション復帰直後は CanMove が剥がれているが、
	// 付け直されるまで動けなくなるので許可する
	if ( !HasStateTag( TAG_State_Player_CanMove ) && !IsHitCancelAirMove() && !IsRecoveryMoveAssistActive() ) return;
	if ( IsHitReacting() ) return;

	if ( !CachedMovementInput.IsNearlyZero() )
	{
		if ( HasStateTag( TAG_State_Player_MoveCancelable ) )
		{
			if ( IsAttacking() )
			{
				CancelAttack();
			}
			if ( IsDodging() )
			{
				CancelDodge();
			}
		}
	}

	FVector2D MovementVector = CachedMovementInput;
	const double CurrentLength = MovementVector.Size();

	static constexpr double BoostedMinInput = 0.4;	// 微小入力でも最低限保証する加速度
	static constexpr double MaxInputLength = 1.0;

	if ( CurrentLength < BoostedMinInput && CurrentLength > UE_KINDA_SMALL_NUMBER )
	{
		MovementVector = MovementVector.GetSafeNormal() * BoostedMinInput;
	}
	else if ( CurrentLength > MaxInputLength )
	{
		MovementVector.Normalize();
	}

	if ( CurrentLength > UE_KINDA_SMALL_NUMBER )
	{
		const bool bBlockLockOnMovement =
			IsDashing() ||
			IsCharging() ||
			IsPlayingChargeAction() ||
			IsHitCancelAirMove();	// 吹き飛びキャンセルの空中移動はロックオンのカニ歩き（対象/向き基準）に吸わせず、カメラ相対の空中制御にする

		if ( !bBlockLockOnMovement )
		{
			if ( CachedLockOnModule &&
				CachedLockOnModule->TryProcessMovementInput( Right, Forward, MovementVector ) )
			{
				return;
			}
		}

		FVector CameraForward, CameraRight;
		GetInputBasis( CameraForward, CameraRight );

		const FVector TargetWorldDirection = ( CameraForward * MovementVector.Y ) + ( CameraRight * MovementVector.X );

		if ( !TargetWorldDirection.IsNearlyZero() )
		{
			if ( IsDodging() ) return;

			// 進行方向と入力方向の内積でブレーキの段階を決める
			bool bIsChargingFullBrake = false;
			bool bIsChargingHalfBrake = false;

			if ( IsCharging() && PlayerParamData && PlayerParamData->bUseChargingBrake )
			{
				if ( const UCharacterMovementComponent* Movement = GetCharacterMovement() )
				{
					if ( !Movement->Velocity.IsNearlyZero() )
					{
						const FVector CurrentVelDir = Movement->Velocity.GetSafeNormal();
						const float InputDot = FVector::DotProduct( CurrentVelDir, TargetWorldDirection );

						if ( InputDot < PlayerParamData->ChargingFullBrakeDotThreshold )
						{
							bIsChargingFullBrake = true;
						}
						else if ( InputDot < PlayerParamData->ChargingHalfBrakeDotThreshold )
						{
							bIsChargingHalfBrake = true;
						}
					}
				}
			}

			const bool bIsTurning = CachedDashModule && CachedDashModule->IsTurning();
			// 攻撃中・チャージダッシュの吸着中は、吸着した向きをレバーで上書きさせない
			// （追従回頭はモジュール側の UpdateChargeDashLockedMovement が行う）
			const bool bIsRotationLockedByAttack = IsAttacking();
			const bool bIsRotationLockedByChargeDashHoming = CachedChargeModuleV2 && CachedChargeModuleV2->IsChargeDashHomingRotationLocked();
			if ( !bIsChargingFullBrake && !bIsTurning && !bIsRotationLockedByAttack && !bIsRotationLockedByChargeDashHoming )
			{
				const FRotator TargetRotation = TargetWorldDirection.Rotation();
				const FRotator CurrentRotation = GetActorRotation();

				float TurnRate = 0.0f;
				if ( const UCharacterMovementComponent* Movement = GetCharacterMovement() )
				{
					TurnRate = Movement->RotationRate.Yaw;

					// 半ブレーキ時は小回りをきかせるため旋回速度を上げる
					if ( bIsChargingHalfBrake && PlayerParamData != nullptr )
					{
						TurnRate *= PlayerParamData->ChargingHalfBrakeRotationSpeedRate;
					}
				}
				const float DeltaTime = GetWorld()->GetDeltaSeconds();

				if ( TurnRate > UE_KINDA_SMALL_NUMBER )
				{
					if ( bIsSurfaceRiding )
					{
						// ワールドの Yaw で回すと姿勢が毎フレーム水平へ戻され、
						// UpdateSurfaceRide の面合わせと綱引きになって震える
						const FVector TargetForward = FVector::VectorPlaneProject( TargetWorldDirection, CurrentGravityUp ).GetSafeNormal();
						if ( !TargetForward.IsNearlyZero() )
						{
							const FQuat TargetQuat = FRotationMatrix::MakeFromZX( CurrentGravityUp, TargetForward ).ToQuat();
							SetActorRotation( FMath::QInterpConstantTo( GetActorQuat(), TargetQuat, DeltaTime,
								FMath::DegreesToRadians( TurnRate ) ) );
						}
					}
					else
					{
						const FRotator NewRotation = FMath::RInterpConstantTo( CurrentRotation, TargetRotation, DeltaTime, TurnRate );
						SetActorRotation( NewRotation );
					}
				}
			}

			if ( IsCharging() )
			{
				// ブレーキ領域では推進力を加えず、UpdateChargingMovementParams の減速制御に任せる
				if ( !bIsChargingFullBrake && !bIsChargingHalfBrake )
				{
					// ヒットキャンセルチャージ直後は推進力を絞り、後退中に自由に動き回れないようにする
					double PropulsionLength = CurrentLength;
					if ( CachedChargeModuleV2 )
					{
						PropulsionLength *= CachedChargeModuleV2->GetHitCancelPropulsionScale();
					}
					AddMovementInput( TargetWorldDirection, PropulsionLength );
				}
			}
			else
			{
				if ( IsFalling() )
				{
					AddMovementInput( TargetWorldDirection, CurrentLength );
				}
				else
				{
					if ( bIsTurning )
					{
						// 意図的に何もしない。滑るような大回りを防ぎ、その場でブレーキを踏んで旋回させる
					}
					else if ( ( CachedDashModule && CachedDashModule->IsInitialTurnBoostActive() ) || IsRecoveryMoveAssistActive() )
					{
						// 回避派生ダッシュ直後・吹き飛び復帰直後は、機体前方だと後ろ向き＋残留 root 速度でもたつく
						AddMovementInput( TargetWorldDirection, CurrentLength );
					}
					else
					{
						AddMovementInput( GetActorForwardVector(), CurrentLength );
					}
				}
			}
		}
	}
}

void ATidePlayerCharacter::RequestMoveEnd()
{
	CachedMovementInput = FVector2D::ZeroVector;

	StopDash();
}

void ATidePlayerCharacter::RequestLook( float Yaw, float Pitch )
{
	if ( !GetController() ) return;

	// どちらも専用カメラが自前で向きを作るため、この間はスティック操作を受け付けない
	if ( IsAirChargeDashCameraSwinging() ) return;
	if ( IsSurfaceRideChaseCameraActive() ) return;

	float FinalYaw = Yaw;
	float FinalPitch = Pitch;

	if ( PlayerParamData )
	{
		FinalYaw *= PlayerParamData->CameraYawSensitivity;
		FinalPitch *= PlayerParamData->CameraPitchSensitivity;

		// ゲームパッドは毎フレーム一定値が積まれ高FPSほど速く回るのでスケールを掛けて揃える。
		// マウス（移動量デルタ）はFPS非依存なので補正しない
		if ( const APlayerController* PC = Cast<APlayerController>( GetController() ) )
		{
			const float StickX = PC->GetInputAnalogKeyState( EKeys::Gamepad_RightX );
			const float StickY = PC->GetInputAnalogKeyState( EKeys::Gamepad_RightY );

			constexpr float GamepadStickDetectThreshold = 0.05f;
			const bool bGamepadLook = ( FMath::Abs( StickX ) + FMath::Abs( StickY ) ) > GamepadStickDetectThreshold;

			if ( bGamepadLook )
			{
				// スロー演出に引きずられないようカメラ系と同じ実時間を使う
				const float RealDelta = FApp::GetDeltaTime();
				const float RefFps = FMath::Max( PlayerParamData->CameraGamepadLookReferenceFps, 1.0f );
				const float FpsScale = RealDelta * RefFps;
				FinalYaw *= FpsScale;
				FinalPitch *= FpsScale;
			}
		}
	}

	AddControllerYawInput( FinalYaw );
	AddControllerPitchInput( FinalPitch );

	if ( FMath::Abs( Yaw ) > 0.01f || FMath::Abs( Pitch ) > 0.01f )
	{
		if ( const UWorld* World = GetWorld() )
		{
			LastCameraInputTime = World->GetTimeSeconds();
		}
	}
}

void ATidePlayerCharacter::RequestJumpStart()
{
	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;
	if ( !HasStateTag( TAG_State_Player_CanMove ) ) return;

	if ( IsCharging() || IsPlayingChargeDash() )
	{
		if ( RequestChargeJump() )
		{
			return;
		}
	}

	// 滑空中のジャンプ入力は「押している間だけ滑空」の操作に使う。
	// ここを通すと自動展開の滑空中に押し直しで空中ジャンプが暴発する
	if ( IsInGlideSession() ) return;

	if ( CachedJumpModule )
	{
		CachedJumpModule->RequestJump();
	}
}

void ATidePlayerCharacter::RequestJumpEnd()
{
	StopJumping();
}

bool ATidePlayerCharacter::TryJumpCancelDuringAction()
{
	if ( HasStateTag( TAG_State_Player_CannotJump ) ) return false;

	// R2 保持中はチャージジャンプ。離していれば実行中アクションの破棄だけ行って false が返る
	if ( IsUsingChargeV2() && CachedChargeModuleV2 && CachedChargeModuleV2->TryChargeAttackCancelJump() )
	{
		return true;
	}

	if ( CachedJumpModule )
	{
		CachedJumpModule->ForceJump( false );
		return true;
	}
	return false;
}

void ATidePlayerCharacter::SetChargeInputHeld( bool bHeld )
{
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->SetChargeInputHeldRaw( bHeld );
	}

	// 空中チャージダッシュ後は R2 長押しでも滑空へ移れる
	if ( CachedGlideModule )
	{
		CachedGlideModule->SetChargeInputHeldRaw( bHeld );
	}
}

void ATidePlayerCharacter::SetJumpInputHeld( bool bHeld )
{
	if ( CachedGlideModule )
	{
		CachedGlideModule->SetJumpInputHeldRaw( bHeld );
	}
}

void ATidePlayerCharacter::RequestChargeActionEnd()
{
	// 死亡・封印状態に関わらず、離したら必ず記録する
	SetChargeInputHeld( false );

	if ( IsDead() ) return;

	// 打ち上げ封印中はその場で発動させない。bLaunchLockDeferChargeRelease が ON なら
	// 凍結保持した溜めのリリースを封印解除時まで後回しにする（OFF なら溜め自体が無いので実質何もしない）
	if ( IsLaunchActionLocked() )
	{
		if ( IsUsingChargeV2() && CachedChargeModuleV2 )
		{
			CachedChargeModuleV2->DeferChargeReleaseForLaunchLock();
		}
		return;
	}

	if ( IsUsingChargeV2() )
	{
		if ( CachedChargeModuleV2 ) CachedChargeModuleV2->ReleaseCharge();
	}
	else
	{
		if ( CachedChargeModule ) CachedChargeModule->ReleaseCharge();
	}
}

void ATidePlayerCharacter::FlushPendingLaunchChargeRelease()
{
	// V1 は封印中ドロップなので V2 のみ
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->FlushPendingChargeRelease();
	}
}

void ATidePlayerCharacter::ResumeHeldChargeAfterLaunchLock()
{
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->ResumeHeldChargeAfterLaunchLock();
	}
}

void ATidePlayerCharacter::RequestDash( bool bIsFromDodge )
{
	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;
	if ( CachedMovementInput.IsNearlyZero() ) return;

	if ( CachedDashModule )
	{
		CachedDashModule->RequestStartDash( bIsFromDodge );
	}
}

void ATidePlayerCharacter::RequestLockOn()
{
	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;
	if ( HasStateTag( TAG_State_Player_CannotLockOn ) ) return;

	if ( !CachedLockOnModule ) return;
	CachedLockOnModule->ToggleLockOn();
}

void ATidePlayerCharacter::RequestLockOnTargetSwitch( float SwitchInput )
{
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;

	if ( !CachedLockOnModule ) return;
	CachedLockOnModule->RequestTargetSwitch( SwitchInput );
}

void ATidePlayerCharacter::RequestChargeCancelStart()
{
	if ( IsDead() ) return;
	if ( !CachedChargeModuleV2 ) return;
	CachedChargeModuleV2->ExecuteManualCancel();
}
void ATidePlayerCharacter::RequestChargeCancelEnd()
{
	if ( !CachedChargeModuleV2 ) return;
	CachedChargeModuleV2->ReleaseManualCancel();
}

void ATidePlayerCharacter::RequestGodActionStart()
{
	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestLockOnStart();
}

void ATidePlayerCharacter::RequestGodActionRelease()
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestLockOnRelease();
}

void ATidePlayerCharacter::RequestGodActionCancel()
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestCancel();
}

bool ATidePlayerCharacter::TryGodArtExecute()
{
	if ( !CachedGodActionModule ) return false;
	if ( !CachedGodActionModule->IsGodBirdModeEnabled() ) return false;
	if ( !CachedGodActionModule->IsGodArtSelecting() ) return false;
	CachedGodActionModule->RequestGodArtExecute();
	return true;
}

void ATidePlayerCharacter::RequestGodArtSelect( int32 Dir )
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestGodArtCycle( Dir );
}

void ATidePlayerCharacter::RequestGodArtSelectAt( int32 ArtIndex )
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestGodArtSelectAt( ArtIndex );
}

bool ATidePlayerCharacter::IsGodArtFaceButtonSelectEnabled() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsFaceButtonSelectEnabled() : false;
}

bool ATidePlayerCharacter::IsGodArtFaceButtonSelectActive() const
{
	return IsGodArtSelecting() && IsGodArtFaceButtonSelectEnabled();
}

void ATidePlayerCharacter::RequestGodArtStickSelect( float AxisX )
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->RequestGodArtStickSelect( AxisX );
}

bool ATidePlayerCharacter::IsGodArtStanceMovementAllowed() const
{
	return PlayerParamData && PlayerParamData->bGodArtStanceAllowMovement;
}

void ATidePlayerCharacter::AddGodActionGauge( float Amount, const FVector& OrbStartWorldLoc, float OrbRadiusScale, float OrbAlphaScale, bool bAnchorOrbToOwner )
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->AddGauge( Amount, OrbStartWorldLoc, OrbRadiusScale, OrbAlphaScale, bAnchorOrbToOwner );
}

void ATidePlayerCharacter::RequestDebugSpawnTornadoInFront()
{
	if ( !UTideGameSettings::Get()->bDebugFlagSpawnTornadoInFront ) return;
	if ( !CachedSlidePassiveModule ) return;

	constexpr float DebugTornadoForwardDistance = 500.0f;	// cm
	CachedSlidePassiveModule->DebugSpawnTornadoInFront( /*bIsLarge=*/ true, DebugTornadoForwardDistance );
}

void ATidePlayerCharacter::RequestCameraReset()
{
	bIsCameraResetRequested = true;
}

void ATidePlayerCharacter::BeginLaunchActionLock()
{
	// 「真下ダイブ＋重力ロック」のアクションは打ち上げと競合してパッド上で無限に跳ね続けるため、
	// BeginLaunchLock より前に必ず片付ける
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->CancelAirDiveActionForLaunch();
	}

	// 滑空は落下速度を毎フレーム上書きするため、畳まないと打ち上げそのものが消える
	// （打ち上げ＝新しい滞空の始まりなので持続時間の予算も張り直す）
	if ( CachedGlideModule )
	{
		CachedGlideModule->ResetForLaunch();
	}

	if ( CachedLaunchLockModule )
	{
		CachedLaunchLockModule->BeginLaunchLock();
	}
}

bool ATidePlayerCharacter::IsLaunchActionLocked() const
{
	return CachedLaunchLockModule && CachedLaunchLockModule->IsLocked();
}

bool ATidePlayerCharacter::IsLaunchAscending() const
{
	return CachedLaunchLockModule && CachedLaunchLockModule->IsAscending();
}

void ATidePlayerCharacter::BeginBoostDash( float MaxSpeed, float Duration, float VelocityCap, float AirZUpSpeed, float StartMontagePlayRate )
{
	if ( !CachedBoostDashModule ) return;

	const float FinalMaxSpeed = ( MaxSpeed > 0.0f )
		? MaxSpeed
		: ( PlayerParamData ? PlayerParamData->BoostDashDefaultMaxSpeed : 0.0f );
	const float FinalDuration = ( Duration > 0.0f )
		? Duration
		: ( PlayerParamData ? PlayerParamData->BoostDashDefaultDuration : 0.0f );

	CachedBoostDashModule->BeginBoost( FinalMaxSpeed, FinalDuration, VelocityCap, AirZUpSpeed, StartMontagePlayRate );
}

bool ATidePlayerCharacter::IsBoostDashing() const
{
	return CachedBoostDashModule && CachedBoostDashModule->IsBoosting();
}

bool ATidePlayerCharacter::IsBoostDashExiting() const
{
	return CachedBoostDashModule && CachedBoostDashModule->IsGroundExiting();
}

float ATidePlayerCharacter::GetSlopeParamForGear( const TArray<float>& ForGear ) const
{
	return UTidePlayerParamDataAsset::GetValueForGear( ForGear, GetCurrentChargeGearIndex() );
}

void ATidePlayerCharacter::UpdateChargingMovementParams( float ChargeRatio, float DriftSteeringScale )
{
	class UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement || !PlayerParamData ) return;

	float SlopeMultiplier = 1.0f;

	const float MaxSlideSpeed = GetSlopeParamForGear( PlayerParamData->MaxSlideSpeedForGear );

	// 坂道補正・滑り落ち・ブレーキ・最終クランプは「上方向＝真上／進行方向＝水平」前提で Velocity の XY を
	// 直接いじるため、面沿いモード中は意味を成さず UpdateSurfaceRide と二重に効いて暴れる
	const bool bSlopeWorldSpaceValid = Movement->IsMovingOnGround() && !bIsSurfaceRiding;

	// 構え中（移動禁止モード）は狙いを定めて止まる場面。「滑らせ続ける」処理（滑り落ち力・摩擦の坂道補正・
	// ドリフトステアリング）を止めて構えブレーキと綱引きさせない
	const bool bStanceLocked = IsGodArtStanceMovementLocked();

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

	// 判定にはブーストのゲート前の生ドリフト条件を使い、継続 t 秒を待たず曲がり始めから効かせる。
	// モジュールの更新順の都合で 1 フレーム前の状態を見るが、ドリフトは複数フレーム続くので影響しない
	const bool bRawDrifting = CachedChargeModuleV2 && CachedChargeModuleV2->IsRawDrifting();
	const bool bDriftSpeedMaintain = PlayerParamData->bEnableDriftSpeedMaintain && bRawDrifting && bSlopeWorldSpaceValid && !bStanceLocked;
	const float DriftSpeedCap = PlayerParamData->DriftSpeedMaxSpeed > 0.0f ? PlayerParamData->DriftSpeedMaxSpeed : MaxSlideSpeed;

	ChargingSpeedPrevOut = ChargingSpeedOut;
	ChargingSpeedIn = Movement->Velocity.Size2D();

	if ( bDriftSpeedMaintain )
	{
		// 開始フレームで維持目標を確保する。毎フレーム現速度を読み直すと摩擦ロスが複利で積んで落ちていく
		if ( !bIsDriftSpeedMaintaining )
		{
			DriftMaintainSpeed = ChargingSpeedIn * PlayerParamData->DriftSpeedMaintainRate;
		}
		DriftMaintainSpeed = FMath::Min( DriftMaintainSpeed + PlayerParamData->DriftSpeedAccelPerSec * DeltaTime, DriftSpeedCap );
	}
	else
	{
		DriftMaintainSpeed = 0.0f;
	}
	bIsDriftSpeedMaintaining = bDriftSpeedMaintain;

	if ( bSlopeWorldSpaceValid )
	{
		FVector MoveDir = Movement->GetLastInputVector();
		const FVector FloorNormal = SmoothedFloorNormal;	// 生の法線はカクついた床で毎フレーム飛ぶ

		const FVector DownhillDir = FVector::VectorPlaneProject( FVector( 0.0f, 0.0f, -1.0f ), FloorNormal ).GetSafeNormal();

		const bool bHasInput = !MoveDir.IsNearlyZero();

		if ( !bHasInput )
		{
			if ( !DownhillDir.IsNearlyZero() )
			{
				MoveDir = DownhillDir;
			}
			else
			{
				MoveDir = GetActorForwardVector();
			}
		}
		else
		{
			MoveDir = MoveDir.GetSafeNormal();
		}

		const FVector SlopeVector = FVector::VectorPlaneProject( MoveDir, FloorNormal ).GetSafeNormal();

		// 倍率と滑り落ち力の両方の基準になるため、ギア別値を一度だけ引いて共有する
		const float DownhillBaseBonus = GetSlopeParamForGear( PlayerParamData->ChargingDownhillBaseBonusForGear );

		float TargetSlopeMultiplier = 1.0f;
		// クランプ範囲は下り／上りで独立して持つ（平地は等倍で通す）
		float ClampMin = 1.0f;
		float ClampMax = 1.0f;

		if ( SlopeVector.Z < 0.0f )	// 下り坂
		{
			const float DescentAngleRad = FMath::Asin( FMath::Abs( SlopeVector.Z ) );
			const float DescentAngleDeg = FMath::RadiansToDegrees( DescentAngleRad );

			const float SteepAngle = GetSlopeParamForGear( PlayerParamData->SteepDescentAngleForGear );

			float TerrainBonus = 0.0f;
			if ( DescentAngleDeg >= SteepAngle )
			{
				TerrainBonus = 1.0f;
			}
			else
			{
				const float AngleRatio = DescentAngleDeg / SteepAngle;
				TerrainBonus = AngleRatio * GetSlopeParamForGear( PlayerParamData->GentleDownhillMaxBonusRateForGear );
				TerrainBonus = FMath::Pow( TerrainBonus, GetSlopeParamForGear( PlayerParamData->DownhillSensitivityForGear ) );
			}

			TargetSlopeMultiplier = 1.0f + ( TerrainBonus * DownhillBaseBonus );
			ClampMin = GetSlopeParamForGear( PlayerParamData->MinChargingDownhillMultiplierForGear );
			ClampMax = GetSlopeParamForGear( PlayerParamData->MaxChargingDownhillMultiplierForGear );
		}
		else if ( SlopeVector.Z > 0.0f )	// 上り坂（下りと対称・専用パラメータで独立調整）
		{
			const float AscentAngleRad = FMath::Asin( FMath::Abs( SlopeVector.Z ) );
			const float AscentAngleDeg = FMath::RadiansToDegrees( AscentAngleRad );

			const float SteepAngle = GetSlopeParamForGear( PlayerParamData->SteepAscentAngleForGear );

			float TerrainPenalty = 0.0f;
			if ( AscentAngleDeg >= SteepAngle )
			{
				TerrainPenalty = 1.0f;
			}
			else
			{
				const float AngleRatio = AscentAngleDeg / SteepAngle;
				TerrainPenalty = AngleRatio * GetSlopeParamForGear( PlayerParamData->GentleUphillMaxRateForGear );
				TerrainPenalty = FMath::Pow( TerrainPenalty, GetSlopeParamForGear( PlayerParamData->UphillSensitivityForGear ) );
			}

			TargetSlopeMultiplier = 1.0f - ( TerrainPenalty * GetSlopeParamForGear( PlayerParamData->ChargingUphillBasePenaltyForGear ) );
			ClampMin = GetSlopeParamForGear( PlayerParamData->MinChargingUphillMultiplierForGear );
			ClampMax = GetSlopeParamForGear( PlayerParamData->MaxChargingUphillMultiplierForGear );
		}

		const float ClampedMultiplier = FMath::Clamp( TargetSlopeMultiplier, ClampMin, ClampMax );

		constexpr float FlatGroundAngleThreshold = 5.0f;	// この角度までは平地としてフェードさせる
		const float SlopeAngleDeg = FMath::RadiansToDegrees( FMath::Acos( FloorNormal.Z ) );
		const float SlopeAlpha = FMath::Clamp( SlopeAngleDeg / FlatGroundAngleThreshold, 0.0f, 1.0f );

		SlopeMultiplier = ApplyChargingSlopeMultiplierInterp( FMath::Lerp( 1.0f, ClampedMultiplier, SlopeAlpha ), DeltaTime );

		// 構え中は補正せず素の値＝グリップさせる
		const float FrictionSlopeDiv = bStanceLocked ? 1.0f : SlopeMultiplier;
		// ドリフト中は摩擦を落とす。CMC は入力方向へ Velocity を内挿して向きを変えるが、角度差のぶん大きさが縮むため
		const float DriftFrictionRate = bDriftSpeedMaintain ? FMath::Clamp( PlayerParamData->DriftGroundFrictionRate, 0.0f, 1.0f ) : 1.0f;
		Movement->GroundFriction = PlayerParamData->ChargingGroundFriction * DriftFrictionRate / FrictionSlopeDiv;
		Movement->BrakingDecelerationWalking = PlayerParamData->ChargingBrakingDecelerationWalking / FrictionSlopeDiv;
		ChargingAppliedGroundFriction = Movement->GroundFriction;

		// 滑り落ちる力は進行方向が坂の下向き成分を持つ分だけ効かせる。
		// 上り坂ではこの力が真逆に働いて減速源になるため、内積が負なら 0 にして慣性を残す
		const float DownhillAlignment = FMath::Max( 0.0f, FVector::DotProduct( MoveDir, DownhillDir ) );
		if ( !bStanceLocked && !DownhillDir.IsNearlyZero() && DownhillAlignment > 0.0f )
		{
			const float SlideGravityRate = GetSlopeParamForGear( PlayerParamData->ChargingDownhillSlideGravityRateForGear );
			const float GravitySlidePower = PlayerParamData->MaxWalkSpeed * DownhillBaseBonus * SlideGravityRate * SlopeAlpha * DownhillAlignment;

			const FVector SlideForce = DownhillDir * ( GravitySlidePower * DeltaTime );
			Movement->Velocity.X += SlideForce.X;
			Movement->Velocity.Y += SlideForce.Y;
		}
	}
	else
	{
		// 面沿い中・空中は等倍。1 フレームで戻すと上限と摩擦が段で変わるので補間して戻す
		SlopeMultiplier = ApplyChargingSlopeMultiplierInterp( 1.0f, DeltaTime );
	}

	const float EasedRatio = FMath::InterpEaseIn( 0.0f, 1.0f, ChargeRatio, PlayerParamData->ChargingEaseExpo );

	const float BaseWalkSpeed = PlayerParamData->MaxWalkSpeed;
	const float StartSpeed = BaseWalkSpeed * PlayerParamData->ChargingWalkSpeedRate;
	const float EndSpeed = BaseWalkSpeed * PlayerParamData->MaxChargingWalkSpeedRate;

	const float CalculatedMaxSpeed = FMath::Lerp( StartSpeed, EndSpeed, EasedRatio ) * SlopeMultiplier;
	Movement->MaxWalkSpeed = FMath::Min( CalculatedMaxSpeed, MaxSlideSpeed );

	// 維持目標が歩行上限を超えている間は上限も引き上げる。超えたままだと CMC のブレーキ経路が
	// 毎フレーム削って維持と綱引きになる
	if ( bDriftSpeedMaintain )
	{
		Movement->MaxWalkSpeed = FMath::Max( Movement->MaxWalkSpeed, FMath::Min( DriftMaintainSpeed, MaxSlideSpeed ) );
	}

	const float StartMinSpeed = PlayerParamData->MinAnalogWalkSpeed * PlayerParamData->ChargingWalkSpeedRate;
	const float EndMinSpeed = PlayerParamData->MinAnalogWalkSpeed * PlayerParamData->MaxChargingWalkSpeedRate;
	Movement->MinAnalogWalkSpeed = FMath::Lerp( StartMinSpeed, EndMinSpeed, EasedRatio ) * SlopeMultiplier;

	const float StartYaw = Movement->IsFalling() ? PlayerParamData->ChargingFallingRotationRateYaw : PlayerParamData->ChargingRotationRateYaw;
	const float EndYaw = PlayerParamData->MaxChargingRotationRateYaw;
	Movement->RotationRate = FRotator( 0.0f, FMath::Lerp( StartYaw, EndYaw, EasedRatio ), 0.0f );

	// ドリフトステアリング（Velocity の向きを強く曲げる）／ブレーキ
	if ( bSlopeWorldSpaceValid && !bStanceLocked && !Movement->Velocity.IsNearlyZero() )
	{
		FVector InputWorldDir = FVector::ZeroVector;
		if ( GetController() )
		{
			const FRotator CameraYawRot( 0.0, GetController()->GetControlRotation().Yaw, 0.0 );
			const FVector CameraForward = FRotationMatrix( CameraYawRot ).GetUnitAxis( EAxis::X );
			const FVector CameraRight = FRotationMatrix( CameraYawRot ).GetUnitAxis( EAxis::Y );
			InputWorldDir = ( CameraForward * CachedMovementInput.Y ) + ( CameraRight * CachedMovementInput.X );
		}

		if ( !InputWorldDir.IsNearlyZero() )
		{
			FVector CurrentVel2D = Movement->Velocity;
			CurrentVel2D.Z = 0.0f;
			const float CurrentSpeed2D = CurrentVel2D.Size();
			FVector CurrentDir2D = CurrentVel2D.GetSafeNormal();

			FVector TargetDir2D = InputWorldDir;
			TargetDir2D.Z = 0.0f;
			TargetDir2D.Normalize();

			const float InputDot = FVector::DotProduct( CurrentDir2D, TargetDir2D );

			const bool bEnableBrake = PlayerParamData->bUseChargingBrake;

			if ( bEnableBrake && InputDot < PlayerParamData->ChargingFullBrakeDotThreshold )
			{
				ResetChargingTurnRamp();
				Movement->Velocity = FMath::VInterpTo( Movement->Velocity, FVector::ZeroVector, DeltaTime, PlayerParamData->ChargingFullBrakeDecelerationSpeed );
			}
			else if ( bEnableBrake && InputDot < PlayerParamData->ChargingHalfBrakeDotThreshold )
			{
				ResetChargingTurnRamp();
				const float DecelSpeed = FMath::FInterpTo( CurrentSpeed2D, 0.0f, DeltaTime, PlayerParamData->ChargingHalfBrakeDecelerationSpeed );
				const FVector NewDir = FMath::VInterpNormalRotationTo( CurrentDir2D, TargetDir2D, DeltaTime, PlayerParamData->ChargingHalfBrakeSteeringSpeed );

				Movement->Velocity.X = NewDir.X * DecelSpeed;
				Movement->Velocity.Y = NewDir.Y * DecelSpeed;
			}
			else
			{
				float DriftSteeringSpeed = PlayerParamData->ChargingDriftSteeringSpeed;

				if ( PlayerParamData->bEnableChargingTurnRamp )
				{
					// ほぼ直進、または旋回方向が反転したら「旋回し始め」からやり直す
					constexpr float TurnAngleDeadZoneDeg = 3.0f;

					const float AngleDeg = FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( InputDot, -1.0f, 1.0f ) ) );

					if ( AngleDeg <= TurnAngleDeadZoneDeg )
					{
						ResetChargingTurnRamp();
					}
					else
					{
						const float Cross = ( CurrentDir2D.X * TargetDir2D.Y ) - ( CurrentDir2D.Y * TargetDir2D.X );
						const float TurnSign = FMath::Sign( Cross );

						if ( !FMath::IsNearlyZero( ChargingTurnContinuousSign ) && TurnSign != ChargingTurnContinuousSign )
						{
							ChargingTurnContinuousTimer = 0.0f;
						}

						ChargingTurnContinuousSign = TurnSign;
						ChargingTurnContinuousTimer += DeltaTime;
					}

					if ( ChargingTurnContinuousTimer > PlayerParamData->ChargingTurnRampMinDuration )
					{
						const float RampElapsed = ChargingTurnContinuousTimer - PlayerParamData->ChargingTurnRampMinDuration;
						DriftSteeringSpeed = FMath::Min(
							DriftSteeringSpeed + ( PlayerParamData->ChargingTurnRampUpRate * RampElapsed ),
							PlayerParamData->ChargingTurnRampMaxSteeringSpeed );

						// キャラ本体の回転力も同じ RampElapsed で増やす（基準を下回らないよう Max で保護）
						if ( PlayerParamData->bEnableChargingTurnRampRotation )
						{
							const float RampedRotationYaw = FMath::Min(
								Movement->RotationRate.Yaw + ( PlayerParamData->ChargingTurnRampRotationUpRate * RampElapsed ),
								PlayerParamData->ChargingTurnRampMaxRotationRateYaw );
							Movement->RotationRate.Yaw = FMath::Max( Movement->RotationRate.Yaw, RampedRotationYaw );
						}
					}
				}

				DriftSteeringSpeed *= DriftSteeringScale;

				const FVector NewDir = FMath::VInterpNormalRotationTo( CurrentDir2D, TargetDir2D, DeltaTime, DriftSteeringSpeed );

				// 坂の滑り落ち等で維持目標を上回っている場合はそちらを優先し、勢いを削らない
				const float AppliedSpeed = bDriftSpeedMaintain
					? FMath::Max( CurrentSpeed2D, FMath::Min( DriftMaintainSpeed, DriftSpeedCap ) )
					: CurrentSpeed2D;

				Movement->Velocity.X = NewDir.X * AppliedSpeed;
				Movement->Velocity.Y = NewDir.Y * AppliedSpeed;
			}
		}
		else
		{
			ResetChargingTurnRamp();
		}
	}

	// MaxWalkSpeed は「入力による加速の上限」に過ぎず、滑り落ち力を Velocity へ直接加算する本処理では
	// 実速度がこれを超えて伸び続ける（摩擦も弱めているため CMC の制動でも戻らない）
	if ( bSlopeWorldSpaceValid )
	{
		const FVector HorizontalVel( Movement->Velocity.X, Movement->Velocity.Y, 0.0f );
		const float HorizontalSpeed = HorizontalVel.Size();
		if ( HorizontalSpeed > MaxSlideSpeed )
		{
			const FVector ClampedVel = HorizontalVel.GetSafeNormal() * MaxSlideSpeed;
			Movement->Velocity.X = ClampedVel.X;
			Movement->Velocity.Y = ClampedVel.Y;
		}
	}

	ChargingSpeedOut = Movement->Velocity.Size2D();
}

float ATidePlayerCharacter::ApplyChargingSlopeMultiplierInterp( float TargetMultiplier, float DeltaTime )
{
	// 坂道倍率は歩行上限（×）と摩擦・ブレーキ（÷）に同時に効くため、1 フレームで切り替えると「ガクッ」になる。
	// 目標は面沿いモードの ON/OFF と上り／下り判定の反転で階段状に飛ぶ
	const float InterpSpeed = PlayerParamData ? PlayerParamData->ChargingSlopeMultiplierInterpSpeed : 0.0f;
	ChargingSlopeMultiplier = InterpSpeed > 0.0f
		? FMath::FInterpTo( ChargingSlopeMultiplier, TargetMultiplier, DeltaTime, InterpSpeed )
		: TargetMultiplier;
	return ChargingSlopeMultiplier;
}

void ATidePlayerCharacter::ResetDriftSpeedMaintain()
{
	bIsDriftSpeedMaintaining = false;
	DriftMaintainSpeed = 0.0f;
	ChargingSpeedIn = 0.0f;
	ChargingSpeedOut = 0.0f;
	ChargingSpeedPrevOut = 0.0f;
	// 前回の滑りで上がった坂道倍率を持ち越すと、開始直後だけ上限が高い（＝摩擦が低い）状態になる
	ChargingSlopeMultiplier = 1.0f;
}

namespace
{
	// 定速補間は最大角速度が固定なので、目標が速く動く（トンネル高速周回）と遅れが定常的に溜まる。
	// 角度差に比例して補間速度を上げる比例制御（ゲイン 0 で従来の定速）
	float SurfaceRideCatchupSpeedDeg( float BaseSpeedDeg, float ErrorDeg, float GainPerDeg )
	{
		return BaseSpeedDeg * ( 1.0f + FMath::Max( 0.0f, ErrorDeg ) * FMath::Max( 0.0f, GainPerDeg ) );
	}

	float AngleBetweenNormalsDeg( const FVector& A, const FVector& B )
	{
		return FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( FVector::DotProduct( A, B ), -1.0f, 1.0f ) ) );
	}
}

void ATidePlayerCharacter::UpdateSurfaceRide( float DeltaTime )
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement || !PlayerParamData ) return;
	if ( !PlayerParamData->bEnableSurfaceRide )
	{
		// 重力・姿勢を触っていた場合だけ通常へ戻す（OFF 経路なので補間しない）
		if ( bIsSurfaceRiding || bSurfaceRidePostureApplied || !CurrentGravityUp.Equals( FVector::UpVector ) )
		{
			bIsSurfaceRiding = false;
			CurrentGravityUp = FVector::UpVector;
			Movement->SetGravityDirection( -FVector::UpVector );
			// 重力だけ戻すと壁・天井に立っていた傾きがそのまま残る
			SetActorRotation( FRotator( 0.0f, GetActorRotation().Yaw, 0.0f ) );
			bSurfaceRidePostureApplied = false;
		}
		// OFF 中は下の予約消化が走らないので捨てる（ON に戻した後の暴発防止）
		SurfaceRideFallMotionRequestTime = -100.0f;
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// 面沿いの各種 early return より前に置く（予約が消化されず残ると後の落下で暴発する）
	UpdateSurfaceRideFallChargeJumpMotion( Now );

	const bool bChargeState = IsCharging() || IsPlayingChargeDash() || IsSurfaceRideChargeHopEntryAllowed();
	const bool bZoneAllowed = !PlayerParamData->bSurfaceRideRequireZone || IsInSurfaceRideZone();

	// 解除は下の分岐を全部通らないと走らないため、通らなかったフレームには理由を残す
	const bool bZoneExitFrame = bWasInSurfaceRideZoneAllowed && !bZoneAllowed;
	const bool bWasRiding = bIsSurfaceRiding;
	bWasInSurfaceRideZoneAllowed = bZoneAllowed;
	bool bZoneExitHandled = false;		// 退出フレームを記録済みか（解除・見送りのどちらでも）
	bool bZoneExitReleased = false;		// 解除まで走ったか（＝チャージジャンプのモーションを流し済み）

	// 面沿いは「急斜面に接地」で始まるが通常の登坂角度では接地できない（鶏と卵）ので下地を作っておく
	ApplyWalkableFloorAngle( PlayerParamData->WalkableFloorAngle );

	bool bShouldRide = false;
	if ( bChargeState )
	{
		if ( Movement->IsMovingOnGround() )
		{
			// 平滑化値は曲面では恒常的に傾斜が浅く出て閾値に届かないため生の法線で判定し、
			// 閾値付近のばたつきは EnterAngle / ExitAngle のヒステリシスで抑える
			const float FloorAngle = GetRawFloorAngleDeg();

			if ( bIsSurfaceRiding && !bZoneAllowed )
			{
				// 急斜面の上でそのまま解除すると溜まった沿面速度が射出になるため、チャージダッシュごと終了して
				// 速度を削る（基準角度で歩ける床なら射出しないので通常解除で足りる）
				bShouldRide = false;
				if ( FloorAngle > PlayerParamData->WalkableFloorAngle )
				{
					HandleSurfaceRideZoneExitRelease();
					bZoneExitHandled = true;
					bZoneExitReleased = true;
				}
				else if ( bZoneExitFrame )
				{
					RecordSurfaceRideZoneExitSkip( FString::Printf(
						TEXT( "接地・床が緩い（%.1f ≤ 登坂 %.1f deg）" ), FloorAngle, PlayerParamData->WalkableFloorAngle ) );
					bZoneExitHandled = true;
				}
			}
			else
			{
				// 入る角度より抜ける角度を小さくしてヒステリシスを作り、境界での点滅を防ぐ
				const float Threshold = bIsSurfaceRiding
					? GetSlopeParamForGear( PlayerParamData->SurfaceRideExitAngleForGear )
					: GetSlopeParamForGear( PlayerParamData->SurfaceRideEnterAngleForGear );

				// 速度は「入るとき」だけ見る。維持中に失速して解除すると急斜面の上で姿勢と重力が戻って
				// 弾き飛ばされるため、緩い床へ降りるまでは維持する
				const bool bSpeedOk = bIsSurfaceRiding
					|| Movement->Velocity.Size() >= GetSlopeParamForGear( PlayerParamData->SurfaceRideMinSpeedForGear );
				bShouldRide = FloorAngle > Threshold && bSpeedOk && bZoneAllowed;
			}

			if ( bShouldRide ) LastSurfaceRideGroundTime = Now;
		}
		else if ( bIsSurfaceRiding && !bZoneAllowed )
		{
			// 接地時と同じ扱いにする。見ないと猶予いっぱい壁面走行が続き、猶予切れの解除は速度もチャージも
			// 触らないため沿面速度がそのまま射出になる（頂上で一瞬浮いて抜ける形が一番出やすい）
			bShouldRide = false;

			// 接地しておらず床角度が取れない。ライド中の上方向＝乗っていた面の法線なので、
			// 世界の真上との角度が接地時の FloorAngle と同じ意味になる
			const float RideSurfaceAngle = AngleBetweenNormalsDeg( CurrentGravityUp, FVector::UpVector );
			if ( RideSurfaceAngle > PlayerParamData->WalkableFloorAngle )
			{
				HandleSurfaceRideZoneExitRelease();
				bZoneExitHandled = true;
				bZoneExitReleased = true;
			}
			else if ( bZoneExitFrame )
			{
				RecordSurfaceRideZoneExitSkip( FString::Printf(
					TEXT( "空中・乗っていた面が緩い（%.1f ≤ 登坂 %.1f deg）" ), RideSurfaceAngle, PlayerParamData->WalkableFloorAngle ) );
				bZoneExitHandled = true;
			}
		}
		else
		{
			// 段差で一瞬浮くたびに解除すると出入りでガタつくため、猶予の間は維持する
			bShouldRide = bIsSurfaceRiding
				&& ( Now - LastSurfaceRideGroundTime <= GetSlopeParamForGear( PlayerParamData->SurfaceRideAirGraceForGear ) );
		}
	}

	bIsSurfaceRiding = bShouldRide;

	// 解除は「チャージ状態 かつ 面沿い中」のフレームでしか走らないため、抜ける前にどちらかが落ちていると
	// 速度もチャージも触られずチャージジャンプも出ない
	if ( bZoneExitFrame && !bZoneExitHandled )
	{
		RecordSurfaceRideZoneExitSkip( !bWasRiding
			? TEXT( "面沿い中でない（退出前に速度・角度不足か空中猶予切れで解除済み）" )
			: TEXT( "チャージ／チャージダッシュ中でない（退出前にチャージが切れている）" ) );
	}

	// エリア内でも、乗っていた面が登坂角度より急なら解除の瞬間に落下が確定する。
	// エリア退出の解除と違い速度もチャージも触らない（射出の心配が無く、チャージは続けられる方が都合が良い）
	if ( bWasRiding && !bShouldRide && !bZoneExitReleased && PlayerParamData->bSurfaceRideFallUseChargeJumpMotion )
	{
		// 接地中に補間値を使うと、ハーフパイプを降り切った瞬間の遅れぶん角度が残って平地でもモーションが出る
		RequestSurfaceRideFallChargeJumpMotion( Movement->IsMovingOnGround()
			? GetRawFloorAngleDeg()
			: AngleBetweenNormalsDeg( CurrentGravityUp, FVector::UpVector ) );
	}

	// 戻り切っているなら姿勢にも重力にも触らない（毎フレーム SetActorRotation すると被弾リアクションや
	// ルートモーションの回転制御と綱引きになる）。重力方向が戻ったかだけで打ち切ってはいけない——重力
	// （VInterpNormalRotationTo）は 1 フレームの回転量に上限が無いが、姿勢（QInterpConstantTo）は約 57°
	// までしか回らず、天井（差 180°）で解除すると残り 120°の傾きが永久に取り残される。
	// bSurfaceRidePostureApplied をフラグで持つのは、面沿い以外が付けた傾き（DIE 等）を起こしにいかないため
	if ( !bShouldRide && !bSurfaceRidePostureApplied && CurrentGravityUp.Equals( FVector::UpVector, 0.001f ) ) return;

	// 目標は「実際に接している面の生法線」にする。平滑化済み法線は接地面から 16〜25°傾いており、
	// そのズレぶん接線速度が毎フレーム地形へ押し込んで CMC のめり込み解消で打ち上げられる
	FVector TargetUp = FVector::UpVector;
	if ( bShouldRide )
	{
		TargetUp = SmoothedFloorNormal.GetSafeNormal();
		if ( PlayerParamData->bSurfaceRideGravityUseRawNormal && Movement->IsMovingOnGround() )
		{
			const FVector RawFloorNormal = Movement->CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
			if ( !RawFloorNormal.IsNearlyZero() )
			{
				TargetUp = RawFloorNormal;
			}
		}
	}
	// スナップさせると姿勢と落下方向が 1 フレームで変わって激しく揺れるため必ず補間する
	const float GravitySpeed = SurfaceRideCatchupSpeedDeg( PlayerParamData->SurfaceRideGravityInterpSpeedDeg,
		AngleBetweenNormalsDeg( CurrentGravityUp, TargetUp ), PlayerParamData->SurfaceRideTrackingCatchupGain );
	CurrentGravityUp = FMath::VInterpNormalRotationTo( CurrentGravityUp, TargetUp, DeltaTime, GravitySpeed );

	if ( !bShouldRide && CurrentGravityUp.Equals( FVector::UpVector, 0.001f ) )
	{
		CurrentGravityUp = FVector::UpVector;
	}

	Movement->SetGravityDirection( -CurrentGravityUp );

	// 姿勢を重力方向へ合わせる。CMC は床探索を重力方向へ行うため、直立のまま壁に立たせると破綻する
	FVector Forward = FVector::VectorPlaneProject( GetActorForwardVector(), CurrentGravityUp ).GetSafeNormal();
	if ( Forward.IsNearlyZero() )
	{
		// 前方が上方向と平行で投影が潰れた（天井からの 180°戻しは必ずこの姿勢を通る）。
		// 右ベクトルをそのまま前方に使うと向きが 90°飛ぶため外積で組み直す
		const FVector Right = FVector::VectorPlaneProject( GetActorRightVector(), CurrentGravityUp ).GetSafeNormal();
		Forward = FVector::CrossProduct( Right, CurrentGravityUp );
	}
	if ( !Forward.IsNearlyZero() )
	{
		const FQuat TargetQuat = FRotationMatrix::MakeFromZX( CurrentGravityUp, Forward ).ToQuat();
		// 戻し中はゲインを掛けない。掛けると天井からの戻しが実質スナップになり
		// 「落下しながら軸が戻る」絵にならない
		const float PostureErrorDeg = FMath::RadiansToDegrees( GetActorQuat().AngularDistance( TargetQuat ) );
		const float PostureSpeed = bShouldRide
			? SurfaceRideCatchupSpeedDeg( PlayerParamData->SurfaceRideRotationInterpSpeedDeg,
				PostureErrorDeg, PlayerParamData->SurfaceRideTrackingCatchupGain )
			: PlayerParamData->SurfaceRideRotationInterpSpeedDeg;
		// QInterpConstantTo の速度はラジアン/秒
		SetActorRotation( FMath::QInterpConstantTo( GetActorQuat(), TargetQuat, DeltaTime,
			FMath::DegreesToRadians( PostureSpeed ) ) );

		bSurfaceRidePostureApplied = bShouldRide || !GetActorUpVector().Equals( FVector::UpVector, 0.001f );
	}

	if ( !bShouldRide ) return;

	// 重力を面法線へ向けると本来の落下が消えるため、本物の重力の面沿い成分を明示的に加える
	const float WorldGravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.0f;
	const FVector WorldGravity( 0.0f, 0.0f, WorldGravityZ * Movement->GravityScale );
	const FVector AlongSurface = FVector::VectorPlaneProject( WorldGravity, CurrentGravityUp );

	// アシストエリアでは戻る力を弱い方の値へ差し替える
	const bool bUphillAssist = IsSurfaceRideUphillAssistActive();
	const float SlideBackScale = bUphillAssist
		? GetSlopeParamForGear( PlayerParamData->SurfaceRideAssistSlideBackScaleForGear )
		: GetSlopeParamForGear( PlayerParamData->SurfaceRideSlideBackScaleForGear );
	Movement->Velocity += AlongSurface * ( SlideBackScale * DeltaTime );

	// UpdateChargingMovementParams のクランプは水平（XY）成分で見るため、壁面で速度がほぼ垂直になると
	// 効かない。面沿い中は総速度で頭打ちにする
	const float MaxSpeed = GetSlopeParamForGear( PlayerParamData->MaxSlideSpeedForGear );

	// 戻る力を弱めるだけでは登りで伸びる手触りにならないので、進行方向の登り成分に比例した加速を足す。
	// チャージダッシュは接線速度をハードセットするためここでの加算は残らず、
	// GetSurfaceRideUphillAssistDashSpeedScale が受け持つ
	if ( bUphillAssist )
	{
		const float AssistAccel = GetSlopeParamForGear( PlayerParamData->SurfaceRideAssistUphillAccelForGear );
		const float AssistMaxSpeed = PlayerParamData->SurfaceRideAssistMaxSpeed > 0.0f
			? PlayerParamData->SurfaceRideAssistMaxSpeed
			: MaxSpeed;
		const FVector TravelDir = FVector::VectorPlaneProject( Movement->Velocity, CurrentGravityUp ).GetSafeNormal();
		const float UphillRate = GetSurfaceRideUphillRate();

		// 下りで出た速度をアシストで更に伸ばさないための蓋
		if ( AssistAccel > 0.0f && UphillRate > 0.0f && !TravelDir.IsNearlyZero()
			&& ( AssistMaxSpeed <= 0.0f || Movement->Velocity.Size() < AssistMaxSpeed ) )
		{
			Movement->Velocity += TravelDir * ( AssistAccel * UphillRate * DeltaTime );
		}
	}

	if ( MaxSpeed > 0.0f && Movement->Velocity.Size() > MaxSpeed )
	{
		Movement->Velocity = Movement->Velocity.GetSafeNormal() * MaxSpeed;
	}
}

void ATidePlayerCharacter::RequestSurfaceRideFallChargeJumpMotion( float RideSurfaceAngleDeg )
{
	if ( !PlayerParamData || IsDead() ) return;

	// 歩ける床で解除したときは落ちないので対象外
	if ( RideSurfaceAngleDeg <= PlayerParamData->WalkableFloorAngle ) return;

	SurfaceRideFallMotionRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void ATidePlayerCharacter::UpdateSurfaceRideFallChargeJumpMotion( float Now )
{
	if ( SurfaceRideFallMotionRequestTime < 0.0f ) return;

	UCharacterMovementComponent* Movement = GetCharacterMovement();

	// 解除フレームでそのまま流してはいけない。解除の瞬間はまだ接地しており、ST→LP は IsFalling() 条件なので、
	// 落下せずに終わるケース（壁の裾で解除等）では ST から出る道が無く ST のポーズで硬直する
	const bool bTimedOut = Now - SurfaceRideFallMotionRequestTime > 0.3f;
	if ( !Movement || !Movement->IsFalling() )
	{
		// 落ちない解除だった。予約を残すと、ずっと後で崖から落ちた瞬間に突然チャージジャンプが出る
		if ( bTimedOut ) SurfaceRideFallMotionRequestTime = -100.0f;
		return;
	}

	SurfaceRideFallMotionRequestTime = -100.0f;
	if ( bTimedOut || IsDead() ) return;

	// チャージダッシュを継続したままの落下は JumpModule に持たせられない（ダッシュ中はあちらがモーション制御を
	// 中断するため ST→LP を繋ぐ持ち主が居なくなる）
	if ( CachedChargeModuleV2 && CachedChargeModuleV2->TryStartChargeDashFallJumpMotion() ) return;

	if ( !CachedJumpModule ) return;

	// 同じ理由で、モーションの持ち主が他に居るときは流さない（ST を張っても相手のモンタージュを潰して終わる）
	if ( IsAttacking() || IsDodging() || IsPlayingChargeAction() || IsCharging() || IsPlayingChargeDash()
		|| IsHitReacting() || IsBoostDashing() || IsInGlideSession() ) return;

	CachedJumpModule->EnterChargeJumpStart();
}

void ATidePlayerCharacter::RecordSurfaceRideZoneExitSkip( const FString& Reason )
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FVector Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;

	SurfaceRideZoneExitRecord = FSurfaceRideZoneExitRecord();
	SurfaceRideZoneExitRecord.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SurfaceRideZoneExitRecord.SurfaceAngleDeg = AngleBetweenNormalsDeg( CurrentGravityUp, FVector::UpVector );
	SurfaceRideZoneExitRecord.SpeedBefore = Velocity.Size();
	SurfaceRideZoneExitRecord.SpeedAfter = Velocity.Size();
	SurfaceRideZoneExitRecord.UpBefore = Velocity.Z;
	SurfaceRideZoneExitRecord.UpAfter = Velocity.Z;
	SurfaceRideZoneExitRecord.bWasAirborne = Movement && !Movement->IsMovingOnGround();
	SurfaceRideZoneExitRecord.bReleased = false;
	SurfaceRideZoneExitRecord.SkipReason = Reason;
}

void ATidePlayerCharacter::HandleSurfaceRideZoneExitRelease()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement || !PlayerParamData ) return;

	SurfaceRideZoneExitRecord.bReleased = true;
	SurfaceRideZoneExitRecord.SkipReason.Reset();

	const FVector Velocity = Movement->Velocity;

	SurfaceRideZoneExitRecord.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SurfaceRideZoneExitRecord.SurfaceAngleDeg = AngleBetweenNormalsDeg( CurrentGravityUp, FVector::UpVector );
	SurfaceRideZoneExitRecord.SpeedBefore = Velocity.Size();
	SurfaceRideZoneExitRecord.UpBefore = Velocity.Z;
	SurfaceRideZoneExitRecord.bWasAirborne = !Movement->IsMovingOnGround();

	// 推進（ロック移動）を先に止める。放置すると落下後も UpdateChargeDashLockedMovement が
	// 毎フレーム水平速度をセットし直して再射出になる
	CancelCharge( false );

	// 速度を入れる前に落下へ移す。接地分岐からも呼ばれるので MOVE_Walking のまま代入すると PhysWalking が
	// 全部持っていく（摩擦・ブレーキで削られ、接地移動は速度を床平面へ投影するので壁から離れる成分も作れない）
	if ( Movement->IsMovingOnGround() )
	{
		Movement->SetMovementMode( MOVE_Falling );
	}

	if ( !PlayerParamData->bSurfaceRideZoneExitSoftRelease )
	{
		Movement->Velocity = FVector::ZeroVector;
		SurfaceRideZoneExitRecord.SpeedAfter = 0.0f;
		SurfaceRideZoneExitRecord.UpAfter = 0.0f;
		return;
	}

	// 射出の本体は上向き成分（壁が急なほど沿面速度は Z へ回る）。倍率を 1.0 で頭打ちにしないのは
	// 「壁を蹴って飛び出す」演出のため——増幅側の歯止めは下の MaxSpeed が持つ
	FVector Released = FVector( Velocity.X, Velocity.Y, 0.0f )
		* FMath::Max( 0.0f, PlayerParamData->SurfaceRideZoneExitHorizontalScale );

	// 下向き（落下）は倍率の対象外。遅くすると浮いて見える
	Released.Z = Velocity.Z > 0.0f
		? Velocity.Z * FMath::Max( 0.0f, PlayerParamData->SurfaceRideZoneExitVerticalScale )
		: Velocity.Z;

	// 倍率だけだと元が速いほど結果も伸びるので飛距離の最悪値を抑える
	const float MaxSpeed = PlayerParamData->SurfaceRideZoneExitMaxSpeed;
	if ( MaxSpeed > 0.0f && Released.SizeSquared() > FMath::Square( MaxSpeed ) )
	{
		Released = Released.GetSafeNormal() * MaxSpeed;
	}

	Movement->Velocity = Released;
	SurfaceRideZoneExitRecord.SpeedAfter = Released.Size();
	SurfaceRideZoneExitRecord.UpAfter = Released.Z;

	// ST から流して ST→LP→ED まで UJumpActionPlayerModule に任せる
	// （LP を直接張ると ST が飛ぶうえ遷移の持ち主が居なくなる）
	if ( PlayerParamData->bSurfaceRideZoneExitUseChargeJumpMotion && !IsDead() && CachedJumpModule )
	{
		CachedJumpModule->EnterChargeJumpStart();
	}
}

void ATidePlayerCharacter::EnterSurfaceRideZone( const FSurfaceRideZoneFlags& Flags )
{
	++SurfaceRideZoneCount;
	if ( Flags.bInvertLateral ) ++SurfaceRideInvertZoneCount;
	if ( Flags.bInvertForward ) ++SurfaceRideInvertForwardZoneCount;
	if ( Flags.bScreenRelative ) ++SurfaceRideScreenRelativeZoneCount;
	if ( Flags.bCameraRoll ) ++SurfaceRideCameraRollZoneCount;
	if ( Flags.bChaseCamera ) ++SurfaceRideChaseZoneCount;
	if ( Flags.bTubeRelative ) ++SurfaceRideTubeRelativeZoneCount;
	if ( Flags.bUphillAssist ) ++SurfaceRideUphillAssistZoneCount;
}

void ATidePlayerCharacter::ExitSurfaceRideZone( const FSurfaceRideZoneFlags& Flags )
{
	SurfaceRideZoneCount = FMath::Max( 0, SurfaceRideZoneCount - 1 );
	if ( Flags.bInvertLateral ) SurfaceRideInvertZoneCount = FMath::Max( 0, SurfaceRideInvertZoneCount - 1 );
	if ( Flags.bInvertForward ) SurfaceRideInvertForwardZoneCount = FMath::Max( 0, SurfaceRideInvertForwardZoneCount - 1 );
	if ( Flags.bScreenRelative ) SurfaceRideScreenRelativeZoneCount = FMath::Max( 0, SurfaceRideScreenRelativeZoneCount - 1 );
	if ( Flags.bCameraRoll ) SurfaceRideCameraRollZoneCount = FMath::Max( 0, SurfaceRideCameraRollZoneCount - 1 );
	if ( Flags.bChaseCamera ) SurfaceRideChaseZoneCount = FMath::Max( 0, SurfaceRideChaseZoneCount - 1 );
	if ( Flags.bTubeRelative ) SurfaceRideTubeRelativeZoneCount = FMath::Max( 0, SurfaceRideTubeRelativeZoneCount - 1 );
	if ( Flags.bUphillAssist ) SurfaceRideUphillAssistZoneCount = FMath::Max( 0, SurfaceRideUphillAssistZoneCount - 1 );
}

bool ATidePlayerCharacter::IsSurfaceRideUphillAssistActive() const
{
	return bIsSurfaceRiding && IsInSurfaceRideUphillAssistZone()
		&& PlayerParamData && PlayerParamData->bEnableSurfaceRideUphillAssist;
}

float ATidePlayerCharacter::GetSurfaceRideUphillRate() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement ) return 0.0f;

	// 真っ直ぐ登りで 1、横切りで 0、下りで負（→0 にクランプ）
	const FVector TravelDir = FVector::VectorPlaneProject( Movement->Velocity, CurrentGravityUp ).GetSafeNormal();
	const FVector DownhillDir = FVector::VectorPlaneProject( -FVector::UpVector, CurrentGravityUp ).GetSafeNormal();
	if ( TravelDir.IsNearlyZero() || DownhillDir.IsNearlyZero() ) return 0.0f;

	return FMath::Max( 0.0f, -FVector::DotProduct( TravelDir, DownhillDir ) );
}

float ATidePlayerCharacter::GetSurfaceRideUphillAssistDashSpeedScale() const
{
	if ( !IsSurfaceRideUphillAssistActive() ) return 1.0f;

	// UpdateSurfaceRide の加速側と同じ蓋
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float AssistMaxSpeed = PlayerParamData->SurfaceRideAssistMaxSpeed > 0.0f
		? PlayerParamData->SurfaceRideAssistMaxSpeed
		: GetSlopeParamForGear( PlayerParamData->MaxSlideSpeedForGear );
	if ( Movement && AssistMaxSpeed > 0.0f && Movement->Velocity.Size() >= AssistMaxSpeed ) return 1.0f;

	// 登り成分に比例させ、平坦では等倍へ戻す（境界で倍率を段にしない）
	const float Scale = GetSlopeParamForGear( PlayerParamData->SurfaceRideAssistDashSpeedScaleForGear );
	return FMath::Lerp( 1.0f, FMath::Max( 1.0f, Scale ), GetSurfaceRideUphillRate() );
}

float ATidePlayerCharacter::GetRawFloorAngleDeg() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement ) return 0.0f;

	const FVector Normal = Movement->CurrentFloor.HitResult.ImpactNormal;
	if ( Normal.IsNearlyZero() ) return 0.0f;

	return FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( Normal.GetSafeNormal().Z, -1.0f, 1.0f ) ) );
}

FVector ATidePlayerCharacter::SampleAveragedFloorNormal( const FVector& InFallbackNormal ) const
{
	if ( !PlayerParamData || !GetWorld() ) return InFallbackNormal;

	// 上方向は面沿い中なら床法線なので、壁面でも「面に沿った前後左右」を正しく取れる
	const FVector Up = CurrentGravityUp;
	const FVector Forward = FVector::VectorPlaneProject( GetActorForwardVector(), Up ).GetSafeNormal();
	if ( Forward.IsNearlyZero() ) return InFallbackNormal;
	const FVector Right = FVector::CrossProduct( Up, Forward ).GetSafeNormal();

	const float Radius = PlayerParamData->FloorNormalSampleRadius;
	const float TraceLength = PlayerParamData->FloorNormalSampleTraceLength;
	const FVector Origin = GetActorLocation();

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery( ECC_WorldStatic );
	ObjParams.AddObjectTypesToQuery( ECC_WorldDynamic );
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( FloorNormalSample ), false, this );

	// 中心は CMC が求めた接地結果を重みに含める
	FVector Sum = InFallbackNormal;
	int32 Count = 1;

	const FVector Offsets[] = { Forward * Radius, -Forward * Radius, Right * Radius, -Right * Radius };
	for ( const FVector& Offset : Offsets )
	{
		const FVector Start = Origin + Offset + Up * ( TraceLength * 0.5f );
		const FVector End = Start - Up * TraceLength;

		FHitResult Hit;
		if ( GetWorld()->LineTraceSingleByObjectType( Hit, Start, End, ObjParams, QueryParams ) )
		{
			Sum += Hit.ImpactNormal;
			++Count;
		}
	}

	if ( Count <= 1 ) return InFallbackNormal;

	const FVector Averaged = ( Sum / static_cast< float >( Count ) ).GetSafeNormal();
	return Averaged.IsNearlyZero() ? InFallbackNormal : Averaged;
}

void ATidePlayerCharacter::UpdateSmoothedFloorNormal( float DeltaTime )
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement ) return;

	// 面沿い中に段差で一瞬浮いたときに上向きへ寄せると、重力方向まで真下へ振れて
	// 猶予中もモードを維持している意味が無くなる
	if ( bIsSurfaceRiding && !Movement->IsMovingOnGround() )
	{
		bWasMovingOnGround = false;
		return;
	}

	// 空中では上向きへ寄せておく。着地時に離陸前の斜面を引きずらないため
	FVector RawNormal = Movement->CurrentFloor.HitResult.ImpactNormal;
	if ( !Movement->IsMovingOnGround() || RawNormal.IsNearlyZero() )
	{
		RawNormal = FVector::UpVector;
	}
	RawNormal = RawNormal.GetSafeNormal();

	// 生法線は接地点 1 点ぶんしか持たずポリゴンを跨ぐたびに飛ぶ。
	// トレース 4 本ぶんのコストがあるので、坂道処理が動くチャージ中だけ走らせる
	if ( PlayerParamData && PlayerParamData->bEnableFloorNormalMultiSample && Movement->IsMovingOnGround()
		&& ( bIsSurfaceRiding || IsCharging() || IsPlayingChargeDash() ) )
	{
		RawNormal = SampleAveragedFloorNormal( RawNormal );
	}

	// 着地の瞬間だけ補間せず即座に合わせる（離陸前の斜面を引きずらないため）。
	// 滑走中はトンネルの床→側壁のように法線が大きく振れても必ず補間して繋ぐ
	const bool bOnGround = Movement->IsMovingOnGround();
	const bool bJustLanded = bOnGround && !bWasMovingOnGround;
	bWasMovingOnGround = bOnGround;

	const float SmoothSpeed = PlayerParamData ? PlayerParamData->FloorNormalSmoothSpeedDeg : 0.0f;
	if ( SmoothSpeed <= 0.0f || bJustLanded )
	{
		SmoothedFloorNormal = RawNormal;
		return;
	}

	// 滑走中に床が飛んだ異常時の保険（既定は大きめで、正当な急変ではスナップしない）
	const float SnapAngle = PlayerParamData ? PlayerParamData->FloorNormalSnapAngle : 0.0f;
	if ( SnapAngle > 0.0f )
	{
		const float DiffDeg = FMath::RadiansToDegrees(
			FMath::Acos( FMath::Clamp( FVector::DotProduct( SmoothedFloorNormal, RawNormal ), -1.0f, 1.0f ) ) );
		if ( DiffDeg >= SnapAngle )
		{
			SmoothedFloorNormal = RawNormal;
			return;
		}
	}

	// トンネルを速く周回すると床法線の角速度が SmoothSpeed を超えて追い付かず、
	// ここが最も遅い段なので下流（重力方向・姿勢）も揃って遅れる
	float EffectiveSmoothSpeed = SmoothSpeed;
	if ( bIsSurfaceRiding && PlayerParamData )
	{
		EffectiveSmoothSpeed = SurfaceRideCatchupSpeedDeg( SmoothSpeed,
			AngleBetweenNormalsDeg( SmoothedFloorNormal, RawNormal ), PlayerParamData->SurfaceRideTrackingCatchupGain );
	}

	SmoothedFloorNormal = FMath::VInterpNormalRotationTo( SmoothedFloorNormal, RawNormal, DeltaTime, EffectiveSmoothSpeed );
}

void ATidePlayerCharacter::ResetChargingTurnRamp()
{
	ChargingTurnContinuousTimer = 0.0f;
	ChargingTurnContinuousSign = 0.0f;
}

void ATidePlayerCharacter::UpdateFrictionRecovery( float RecoveryRate )
{
	if ( PlayerParamData == nullptr ) return;

	if ( class UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		const float EasedRate = FMath::InterpEaseIn( 0.0f, 1.0f, RecoveryRate, PlayerParamData->ChargeActionFrictionRecoveryEaseExpo );

		Movement->GroundFriction = FMath::Lerp( RecoveryStartFriction, PlayerParamData->GroundFriction, EasedRate );
		Movement->BrakingDecelerationWalking = FMath::Lerp( RecoveryStartBrakingWalking, PlayerParamData->BrakingDecelerationWalking, EasedRate );

		if ( Movement->IsFalling() )
		{
			Movement->AirControl = FMath::Lerp( RecoveryStartAirControl, PlayerParamData->AirControl, EasedRate );
			Movement->GravityScale = FMath::Lerp( RecoveryStartGravityScale, PlayerParamData->GravityScale, EasedRate );
			Movement->BrakingDecelerationFalling = FMath::Lerp( RecoveryStartBrakingFalling, PlayerParamData->BrakingDecelerationFalling, EasedRate );
		}
		else
		{
			Movement->AirControl = PlayerParamData->AirControl;
			Movement->GravityScale = PlayerParamData->GravityScale;
			Movement->BrakingDecelerationFalling = PlayerParamData->BrakingDecelerationFalling;
		}
	}
}

void ATidePlayerCharacter::RequestAttack( EPlayerAttackType AttackType, bool bForce )
{
	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;
	if ( !CachedAttackModule ) return;
	CachedAttackModule->RequestAttack( AttackType, bForce );
}

bool ATidePlayerCharacter::RequestChargeJump()
{
	if ( IsDead() ) return false;
	if ( IsUsingChargeV2() )
	{
		if ( CachedChargeModuleV2 ) return CachedChargeModuleV2->RequestChargeJump();
	}
	else
	{
		if ( CachedChargeModule ) return CachedChargeModule->RequestChargeJump();
	}
	return false;
}

void ATidePlayerCharacter::RequestShiftUpChargeGear()
{
	if ( IsUsingChargeV2() )
	{
		if ( CachedChargeModuleV2 )
		{
			CachedChargeModuleV2->ShiftUpGear();
		}
	}
}

void ATidePlayerCharacter::RequestChargeGearDown()
{
	if ( CachedChargeModuleV2 ) CachedChargeModuleV2->ShiftDownGear();
}

void ATidePlayerCharacter::RequestChargeGearUp()
{
	if ( CachedChargeModuleV2 ) CachedChargeModuleV2->ShiftUpGear( true );
}

bool ATidePlayerCharacter::ForceMaxChargeGear()
{
	return CachedChargeModuleV2 ? CachedChargeModuleV2->SetGearToMax() : false;
}

void ATidePlayerCharacter::NotifyChargeDriftSparkBurst()
{
	if ( CachedChargeModuleV2 ) CachedChargeModuleV2->NotifyDriftSparkForcedBurst();
}

void ATidePlayerCharacter::ArmGustChargeBuff()
{
	if ( CachedChargeModuleV2 ) CachedChargeModuleV2->ArmGustChargeBuff();
}

bool ATidePlayerCharacter::IsGustChargeBuffArmed() const
{
	return CachedChargeModuleV2 ? CachedChargeModuleV2->IsGustChargeBuffArmed() : false;
}

void ATidePlayerCharacter::ClearGustChargeBuff()
{
	if ( CachedChargeModuleV2 ) CachedChargeModuleV2->ClearGustChargeBuff();
}

bool ATidePlayerCharacter::IsGustBuffedChargeActionActive() const
{
	return CachedChargeModuleV2 ? CachedChargeModuleV2->IsGustBuffedActionActive() : false;
}

void ATidePlayerCharacter::ReserveAutoDashOnLanding()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->ReserveAutoDashOnLanding();
}

void ATidePlayerCharacter::EnterJumpFallingLoop()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->EnterFallingLoop();
}

void ATidePlayerCharacter::EnterJumpLandingEnd()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->EnterLandingEnd();
}

void ATidePlayerCharacter::ClearJumpAndLandingDash()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->ClearJumpAndLandingDash();
}

void ATidePlayerCharacter::StartJumpWindTrailEffect()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->StartWindTrailEffect();
}

void ATidePlayerCharacter::RequestPushFirstJumpCamera()
{
	RequestPushJumpCamera( TEXT( "FirstJump" ) );
}

void ATidePlayerCharacter::RequestPushJumpCamera( FName CameraRowName )
{
	// FirstJump とギア別で共通のハンドルを使うため、二重 Push を防ぐ
	if ( FirstJumpCameraHandle.IsValid() ) return;

	if ( const APlayerController* PC = Cast<APlayerController>( GetController() ) )
	{
		if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
		{
			if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
			{
				FirstJumpCameraHandle = CameraSubsystem->PushThirdPersonCameraByKey( CameraRowName );
			}
		}
	}
}

void ATidePlayerCharacter::RequestPopFirstJumpCamera()
{
	if ( !FirstJumpCameraHandle.IsValid() ) return;
	if ( const APlayerController* PC = Cast<APlayerController>( GetController() ) )
	{
		if ( ULocalPlayer* LocalPlayer = PC->GetLocalPlayer() )
		{
			if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
			{
				CameraSubsystem->PopCameraMode( FirstJumpCameraHandle );
			}
		}
	}
	FirstJumpCameraHandle.Clear();
}

void ATidePlayerCharacter::ForceStartDash( bool bIsFromDodge )
{
	// 着地時の予約ダッシュが、落下中に死亡したあとの着地で発火して死亡をキャンセルするのを防ぐ
	if ( IsDead() ) return;
	if ( !CachedDashModule ) return;
	CachedDashModule->ForceStartDash( bIsFromDodge );
}

void ATidePlayerCharacter::StopDash()
{
	if ( !CachedDashModule ) return;
	CachedDashModule->StopDash();
}

void ATidePlayerCharacter::StartDashInertiaDecayFromSpeed( float StartSpeed )
{
	if ( !CachedDashModule ) return;
	CachedDashModule->StartInertiaDecayFromSpeed( StartSpeed );
}

void ATidePlayerCharacter::CancelCharge( bool bRestoreDash )
{
	if ( IsUsingChargeV2() )
	{
		if ( CachedChargeModuleV2 ) CachedChargeModuleV2->CancelCharge( bRestoreDash );
	}
	else
	{
		if ( CachedChargeModule ) CachedChargeModule->CancelCharge( bRestoreDash );
	}
}

void ATidePlayerCharacter::NotifyChargeInterruptedByDamage()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return;
	CachedChargeModuleV2->NotifyChargeInterruptedByDamage();
}

void ATidePlayerCharacter::NotifyGodArtStanceInterruptedByDamage()
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->NotifyOwnerDamaged();
}

bool ATidePlayerCharacter::PauseChargeDashForBoost()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->PauseChargeDashForBoost();
}

bool ATidePlayerCharacter::CancelChargeJumpForBoost()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->CancelChargeJumpForBoost();
}

bool ATidePlayerCharacter::ResumeChargeDashAfterBoost()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->ResumeChargeDashAfterBoost();
}

bool ATidePlayerCharacter::PauseChargeForBoost()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->PauseChargeForBoost();
}

bool ATidePlayerCharacter::ResumeChargeAfterBoost()
{
	if ( !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->ResumeChargeAfterBoost();
}

void ATidePlayerCharacter::CancelAttack()
{
	if ( !CachedAttackModule ) return;
	CachedAttackModule->CancelAttack();
}

void ATidePlayerCharacter::CancelDodge()
{
	if ( !CachedDodgeModule ) return;
	CachedDodgeModule->CancelDodge();
}

void ATidePlayerCharacter::ForceJump( bool bOverrideXY )
{
	if ( IsDead() ) return;
	if ( !CachedJumpModule ) return;
	CachedJumpModule->ForceJump( bOverrideXY );
}

bool ATidePlayerCharacter::IsHitCancelAirMove() const
{
	return CachedJumpModule && CachedJumpModule->IsHitCancelAirMove();
}

bool ATidePlayerCharacter::HasJumpedThisAirtime() const
{
	return CachedJumpModule && CachedJumpModule->HasJumpedThisAirtime();
}

void ATidePlayerCharacter::NotifyHitReactionRecovered()
{
	if ( const UWorld* World = GetWorld() )
	{
		LastHitReactionRecoveredTime = World->GetTimeSeconds();
	}
}

bool ATidePlayerCharacter::IsRecoveryMoveAssistActive() const
{
	if ( !PlayerParamData || PlayerParamData->RecoveryTurnSuppressTime <= 0.0f ) return false;
	const UWorld* World = GetWorld();
	if ( !World ) return false;
	return ( World->GetTimeSeconds() - LastHitReactionRecoveredTime ) < PlayerParamData->RecoveryTurnSuppressTime;
}

void ATidePlayerCharacter::ForceDodge()
{
	if ( !CachedDodgeModule ) return;
	CachedDodgeModule->ForceDodge();
}

void ATidePlayerCharacter::CancelAllActions()
{
	StopDash();

	CancelCharge( false );

	if ( CachedAttackModule )
	{
		CachedAttackModule->CancelAttack();
	}

	if ( CachedDodgeModule )
	{
		CachedDodgeModule->CancelDodge();
	}

	RefreshMovementParams();
}

void ATidePlayerCharacter::ConsumeAirJumps()
{
	if ( !CachedJumpModule ) return;
	CachedJumpModule->ConsumeAirJumps();
}

bool ATidePlayerCharacter::ConsumeCameraResetRequest()
{
	if ( bIsCameraResetRequested )
	{
		bIsCameraResetRequested = false;
		return true;
	}
	return false;
}

bool ATidePlayerCharacter::IsLockOnActive() const
{
	if ( !CachedLockOnModule ) return false;
	return CachedLockOnModule->IsLockOnActive();
}

bool ATidePlayerCharacter::IsFalling() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if ( !Movement ) return false;
	return Movement->IsFalling();
}

bool ATidePlayerCharacter::IsDodging() const
{
	if ( !CachedDodgeModule ) return false;
	return CachedDodgeModule->IsDodging();
}

bool ATidePlayerCharacter::IsInvincible() const
{
	if ( !CachedHitReactionModule ) return false;
	return CachedHitReactionModule->IsInvincible();
}

bool ATidePlayerCharacter::IsHitReacting() const
{
	if ( CachedHitReactionModule && CachedHitReactionModule->IsReacting() ) return true;

	// HasStateTag（階層マッチ）だと被弾無敵の子タグ（HitReaction.Immune）が引っかかり、早期キャンセルしても
	// 無敵時間が切れるまで移動・アクションが全部ブロックされるので完全一致で見る
	if ( const UStateTagComponent* TagComp = FindComponentByClass<UStateTagComponent>() )
	{
		return TagComp->GetActiveStateTags().HasTagExact( TAG_State_Common_HitReaction );
	}
	return false;
}

bool ATidePlayerCharacter::IsAttacking() const
{
	if ( !CachedAttackModule ) return false;
	return CachedAttackModule->IsAttacking();
}

bool ATidePlayerCharacter::IsDashing() const
{
	if ( !CachedDashModule ) return false;
	return CachedDashModule->IsDashing();
}

bool ATidePlayerCharacter::IsCharging() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsCharging() : false;
	}
	return CachedChargeModule ? CachedChargeModule->IsCharging() : false;
}

bool ATidePlayerCharacter::IsPlayingChargeAction() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsPlayingChargeAction() : false;
	}
	return CachedChargeModule ? CachedChargeModule->IsPlayingChargeAction() : false;
}

bool ATidePlayerCharacter::IsPlayingChargeDash() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsPlayingChargeDash() : false;
	}
	return CachedChargeModule ? CachedChargeModule->IsPlayingChargeDash() : false;
}

bool ATidePlayerCharacter::IsChargeDashEndMovementLocked() const
{
	// チャージダッシュEDの硬直はV2のみの概念
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsChargeDashEndMovementLocked() : false;
	}
	return false;
}

bool ATidePlayerCharacter::IsAirChargeDashing() const
{
	// 空中チャージダッシュはV2のみの概念（V1には存在しない）
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsAirChargeDashing() : false;
	}
	return false;
}

float ATidePlayerCharacter::GetChargeDashRotationRateYaw() const
{
	if ( !PlayerParamData ) return 0.0f;

	if ( IsAirChargeDashing() && PlayerParamData->AirChargeDashRotationRateYaw > 0.0f )
	{
		return PlayerParamData->AirChargeDashRotationRateYaw;
	}
	return PlayerParamData->ChargeDashRotationRateYaw;
}

bool ATidePlayerCharacter::IsAirActionLimitedAfterAirCharge() const
{
	if ( !IsFalling() || !IsUsingChargeV2() || !CachedChargeModuleV2 ) return false;
	return CachedChargeModuleV2->IsAirChargeDashExhausted() ||
		CachedChargeModuleV2->IsAirChargeAttackUsedThisAirtime();
}

bool ATidePlayerCharacter::IsGroundNormalAttackMasked() const
{
	if ( !PlayerParamData || PlayerParamData->bEnableGroundNormalAttack ) return false;
	return !IsFalling();	// 空中の振り下ろし（Charged 種別）はマスク対象外
}

bool ATidePlayerCharacter::IsPlayingChargeJump() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsPlayingChargeJump() : false;
	}
	return CachedChargeModule ? CachedChargeModule->IsPlayingChargeJump() : false;
}

bool ATidePlayerCharacter::IsChargeJumpNormalAttackWindow() const
{
	return CachedChargeModuleV2 && IsUsingChargeV2() ? CachedChargeModuleV2->IsChargeJumpNormalAttackWindow() : false;
}

bool ATidePlayerCharacter::IsPlayingChargeHopJump() const
{
	// チャージホップはV2のみの概念（V1には存在しない）
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsPlayingChargeHopJump() : false;
	}
	return false;
}

bool ATidePlayerCharacter::IsPlayingChargeAttack() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->IsPlayingChargeAttack() : false;
	}
	return CachedChargeModule ? CachedChargeModule->IsPlayingChargeAttack() : false;
}

bool ATidePlayerCharacter::IsGodActionActive() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsActive() : false;
}

bool ATidePlayerCharacter::IsGodFrolicActive() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsFrolicActive() : false;
}

bool ATidePlayerCharacter::IsGodGuidanceActive() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsGuidanceActive() : false;
}

bool ATidePlayerCharacter::StartGodBirdTornadoEscort( const FVector& TornadoCenter )
{
	return CachedGodBirdModule ? CachedGodBirdModule->StartTornadoEscort( TornadoCenter ) : false;
}

bool ATidePlayerCharacter::IsGodBirdShownByPlayerAction() const
{
	return CachedGodBirdModule ? CachedGodBirdModule->IsShownByPlayerAction() : false;
}

bool ATidePlayerCharacter::IsGodArtSelecting() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsGodArtSelecting() : false;
}

bool ATidePlayerCharacter::IsGodArtStanceMovementLocked() const
{
	return IsGodArtSelecting() && PlayerParamData && !PlayerParamData->bGodArtStanceAllowMovement;
}

bool ATidePlayerCharacter::IsGodActionExecuting() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsExecuting() : false;
}

bool ATidePlayerCharacter::IsGodActionLockingOn() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsLockingOn() : false;
}

bool ATidePlayerCharacter::IsGodSlashWideCutLoopPassthrough() const
{
	return CachedGodActionModule ? CachedGodActionModule->IsGodSlashWideCutLoopPassthrough() : false;
}

bool ATidePlayerCharacter::GetGodSlashCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const
{
	return CachedGodActionModule
		? CachedGodActionModule->GetCameraFraming( OutFocusLocation, OutTargetCenter, OutTargetRadius, OutCutIndex, bOutCutLanded, bOutWideCut, bOutWideCutFinishing )
		: false;
}

bool ATidePlayerCharacter::GetAirChargeDashCameraFraming( FRotator& OutTargetRotation ) const
{
	if ( !bAirChargeDashCamFramingValid ) return false;
	OutTargetRotation = AirChargeDashCamTargetRotation;
	return true;
}

void ATidePlayerCharacter::SetAirChargeDashCameraTarget( const FRotator& InTargetRotation )
{
	AirChargeDashCamTargetRotation = InTargetRotation;
	bAirChargeDashCamFramingValid = true;
	bAirChargeDashCamInputLocked = true;	// 寄せ演出開始＝カメラ入力ロック（寄せ完了時にモジュールが解除する）
}

void ATidePlayerCharacter::ClearAirChargeDashCameraFraming()
{
	bAirChargeDashCamFramingValid = false;
	bAirChargeDashCamInputLocked = false;
}

bool ATidePlayerCharacter::IsDead() const
{
	return bIsDead;
}

bool ATidePlayerCharacter::HasDodgeInputBuffered() const
{
	if ( !CachedDodgeModule ) return false;
	return CachedDodgeModule->HasDodgeInputBuffered();
}

bool ATidePlayerCharacter::HasRecoveryInput() const
{
	if ( APlayerController* PC = Cast<APlayerController>( GetController() ) )
	{
		if ( PC->WasInputKeyJustPressed( EKeys::AnyKey ) )
		{
			return true;
		}
	}

	return false;
}

float ATidePlayerCharacter::GetCurrentRotationRateYaw() const
{
	if ( const UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		return Movement->RotationRate.Yaw;
	}
	return 0.0f;
}

void ATidePlayerCharacter::SetCustomTimeDilation( float TimeDilation )
{
	CustomTimeDilation = TimeDilation;
}

void ATidePlayerCharacter::StopVelocity()
{
	if ( UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		Movement->Velocity.X = 0.0f;
		Movement->Velocity.Y = 0.0f;
	}

}

void ATidePlayerCharacter::CaptureFrictionRecoveryStartParams()
{
	if ( UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		RecoveryStartFriction = Movement->GroundFriction;
		RecoveryStartBrakingWalking = Movement->BrakingDecelerationWalking;
		RecoveryStartBrakingFalling = Movement->BrakingDecelerationFalling;
		RecoveryStartAirControl = Movement->AirControl;
		RecoveryStartGravityScale = Movement->GravityScale;
	}
}

float ATidePlayerCharacter::CalculateDamage( float DamageMultiplier )
{
	const float Base = StatusComponent ? StatusComponent->GetBaseAttackPower() : 1.0f;
	return Base * DamageMultiplier;
}

bool ATidePlayerCharacter::IsCameraInputActive() const
{
	if ( !PlayerParamData ) return false;
	if ( const UWorld* World = GetWorld() )
	{
		return ( World->GetTimeSeconds() - LastCameraInputTime ) < PlayerParamData->ChargeCameraCenteringDelay;
	}
	return false;
}

void ATidePlayerCharacter::ResetCameraInputActivity()
{
	// 最後のカメラ操作時刻を十分過去へ巻き戻して IsCameraInputActive() を即 false にする
	if ( const UWorld* World = GetWorld() )
	{
		const float Delay = PlayerParamData ? PlayerParamData->ChargeCameraCenteringDelay : 0.0f;
		LastCameraInputTime = World->GetTimeSeconds() - Delay - 1.0f;
	}
	else
	{
		LastCameraInputTime = -100.0f;
	}
}

FVector2D ATidePlayerCharacter::GetAllowedMovementInput() const
{
	// 打ち上げ中は Disable でも移動（空中制御）だけは許可する
	const bool bActionDisabled = HasStateTag( TAG_State_Common_Disable ) && !IsLaunchActionLocked();
	if ( bActionDisabled || !HasStateTag( TAG_State_Player_CanMove ) )
	{
		return FVector2D::ZeroVector;
	}

	return CachedMovementInput;
}

void ATidePlayerCharacter::BeginMovementSpikeFrame( float DeltaTime )
{
	// フレーム順は「アクターの Tick → CMC の移動」なので、前フレーム末から今フレーム頭までの差分が
	// CMC が変えた分になる。速度が変わらず実移動量だけ跳ねるなら CMC の接地移動側（斜面・段差・床スナップ）
	const float Threshold = PlayerParamData ? PlayerParamData->MovementSpikeLogThresholdSpeed : 0.0f;
	bMovementSpikeFrameValid = Threshold > 0.0f && DeltaTime > 0.0f;
	if ( !bMovementSpikeFrameValid ) return;

	// 位置差分が生まれたのは前フレームの CMC の移動なので、割るのは前フレームの DeltaTime
	// （今フレームの値だとフレーム時間が揺れるだけで偽のズレが出る）
	const FVector Location = GetActorLocation();
	MovementSpikeActualMove = ( MovementSpikePrevLocation.IsZero() || MovementSpikePrevDeltaTime <= 0.0f )
		? FVector::ZeroVector
		: ( Location - MovementSpikePrevLocation ) / MovementSpikePrevDeltaTime;
	MovementSpikeActualSpeed = MovementSpikeActualMove.Size();

	// 前フレーム末から今フレーム頭までの移動＝CMC とルートモーションの担当分。
	// 残りが Tick 内で自前に位置を押した分（敵との押し出し・風・地面の吸い込み）になる
	MovementSpikeOutsideTickSpeed = ( MovementSpikePrevTickEndLocation.IsZero() || MovementSpikePrevDeltaTime <= 0.0f )
		? 0.0f
		: ( Location - MovementSpikePrevTickEndLocation ).Size() / MovementSpikePrevDeltaTime;

	MovementSpikePrevLocation = Location;
	MovementSpikePrevDeltaTime = DeltaTime;

	MovementSpikeVelIn = GetVelocity();
	MovementSpikeVelAfterModules = MovementSpikeVelIn;
}

void ATidePlayerCharacter::MarkMovementSpikeMidTick()
{
	if ( !bMovementSpikeFrameValid ) return;

	MovementSpikeMidTickLocation = GetActorLocation();
	MovementSpikeVelAfterRide = GetVelocity();
}

void ATidePlayerCharacter::EndMovementSpikeFrame( float DeltaTime )
{
	if ( !bMovementSpikeFrameValid ) return;

	const FVector VelOut = GetVelocity();

	// ラッチしないフレームでも基準は必ず更新する
	const FVector TickEndLocation = GetActorLocation();
	const float InvDelta = DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f;
	const float MoveModulesSpeed = ( MovementSpikeMidTickLocation - MovementSpikePrevLocation ).Size() * InvDelta;
	const float MovePushWindSpeed = ( TickEndLocation - MovementSpikeMidTickLocation ).Size() * InvDelta;
	MovementSpikePrevTickEndLocation = TickEndLocation;

	const float SpeedIn = MovementSpikeVelIn.Size();
	const float DeltaCmc = SpeedIn - MovementSpikeVelPrevOut.Size();
	const float DeltaModules = MovementSpikeVelAfterModules.Size() - SpeedIn;
	const float DeltaRide = MovementSpikeVelAfterRide.Size() - MovementSpikeVelAfterModules.Size();
	const float DeltaPushWind = VelOut.Size() - MovementSpikeVelAfterRide.Size();

	// 大きさが変わらない急な方向転換は上の差分に出ない（「一瞬あらぬ方向へ動く」はこちらでしか捕まらない）
	auto TurnAngleDeg = []( const FVector& A, const FVector& B )
	{
		if ( A.SizeSquared() < 100.0f || B.SizeSquared() < 100.0f ) return 0.0f;
		return FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp(
			static_cast< float >( FVector::DotProduct( A.GetSafeNormal(), B.GetSafeNormal() ) ), -1.0f, 1.0f ) ) );
	};
	const float TurnCmc = TurnAngleDeg( MovementSpikeVelPrevOut, MovementSpikeVelIn );
	const float TurnModules = TurnAngleDeg( MovementSpikeVelIn, MovementSpikeVelAfterModules );
	const float TurnRide = TurnAngleDeg( MovementSpikeVelAfterModules, MovementSpikeVelAfterRide );
	const float TurnPushWind = TurnAngleDeg( MovementSpikeVelAfterRide, VelOut );

	MovementSpikeVelPrevOut = VelOut;

	// めり込み → CMC の押し出し → 打ち上げ、という並びなので押し出された後のフレームを見ても原因が分からない。
	// 毎フレーム重なり問い合わせ 1 回ぶんのコストを払って必ず残す
	bool bPenetrating = false;
	float PenetrationDepth = 0.0f;
	FString PenetrationName;
	if ( const UCapsuleComponent* Capsule = GetCapsuleComponent() )
	{
		const FCollisionShape Shape = Capsule->GetCollisionShape();
		const FVector CapsuleLoc = Capsule->GetComponentLocation();
		const FQuat CapsuleQuat = Capsule->GetComponentQuat();

		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery( ECC_WorldStatic );
		ObjParams.AddObjectTypesToQuery( ECC_WorldDynamic );
		FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( MovementSpikePenetration ), false, this );

		TArray<FOverlapResult> Overlaps;
		if ( GetWorld()->OverlapMultiByObjectType( Overlaps, CapsuleLoc, CapsuleQuat, ObjParams, Shape, QueryParams ) )
		{
			for ( const FOverlapResult& Overlap : Overlaps )
			{
				UPrimitiveComponent* Comp = Overlap.GetComponent();
				if ( !Comp ) continue;

				// トリガーボリューム（オーバーラップ専用）は CMC のめり込み解消の対象にならず偽陽性になる
				if ( Comp->GetCollisionResponseToChannel( Capsule->GetCollisionObjectType() ) != ECR_Block ) continue;
				bPenetrating = true;

				// 三角メッシュ相手では MTD が取れないが、重なりの事実だけで足りる
				FMTDResult MTD;
				if ( Comp->ComputePenetration( MTD, Shape, CapsuleLoc, CapsuleQuat ) && MTD.Distance > PenetrationDepth )
				{
					PenetrationDepth = MTD.Distance;
					PenetrationName = Comp->GetOwner() ? Comp->GetOwner()->GetName() : Comp->GetName();
				}
				else if ( PenetrationName.IsEmpty() )
				{
					PenetrationName = Comp->GetOwner() ? Comp->GetOwner()->GetName() : Comp->GetName();
				}
			}
		}
	}

	// 跳ね上がりの原因（上向き速度を足されたのか、面が落ちて離れただけか）は離脱フレームの状態にしか出ない
	const bool bOnGroundNow = GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	const bool bSeparated = bMovementSpikePrevOnGround && !bOnGroundNow;
	bMovementSpikePrevOnGround = bOnGroundNow;

	const float Threshold = PlayerParamData->MovementSpikeLogThresholdSpeed;
	const float TurnThreshold = FMath::Max( 1.0f, PlayerParamData->MovementSpikeLogThresholdTurnDeg );
	const bool bActualMismatch = SpeedIn > 100.0f && FMath::Abs( MovementSpikeActualSpeed - SpeedIn ) > Threshold;
	const bool bSpeedSpike = FMath::Abs( DeltaCmc ) >= Threshold || FMath::Abs( DeltaModules ) >= Threshold
		|| FMath::Abs( DeltaRide ) >= Threshold || FMath::Abs( DeltaPushWind ) >= Threshold;
	const bool bTurnSpike = TurnCmc >= TurnThreshold || TurnModules >= TurnThreshold
		|| TurnRide >= TurnThreshold || TurnPushWind >= TurnThreshold;

	if ( !bSpeedSpike && !bTurnSpike && !bActualMismatch && !bSeparated && !bPenetrating ) return;

	const UCharacterMovementComponent* Movement = GetCharacterMovement();

	FMovementSpikeSample Sample;
	Sample.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Sample.DeltaTimeMs = DeltaTime * 1000.0f;
	Sample.DeltaCmc = DeltaCmc;
	Sample.DeltaModules = DeltaModules;
	Sample.DeltaRide = DeltaRide;
	Sample.DeltaPushWind = DeltaPushWind;
	Sample.TurnCmc = TurnCmc;
	Sample.TurnModules = TurnModules;
	Sample.TurnRide = TurnRide;
	Sample.TurnPushWind = TurnPushWind;
	Sample.SpeedIn = SpeedIn;
	Sample.ActualSpeed = MovementSpikeActualSpeed;
	Sample.FloorAngleDeg = GetRawFloorAngleDeg();
	Sample.WalkableAngle = Movement ? Movement->GetWalkableFloorAngle() : 0.0f;
	Sample.MaxWalkSpeed = Movement ? Movement->MaxWalkSpeed : 0.0f;
	Sample.ZoneCount = SurfaceRideZoneCount;
	Sample.bRiding = bIsSurfaceRiding;
	Sample.bOnGround = Movement ? Movement->IsMovingOnGround() : false;
	Sample.bSeparated = bSeparated;
	Sample.FloorDist = Movement ? Movement->CurrentFloor.FloorDist : 0.0f;
	Sample.MaxStepHeight = Movement ? Movement->MaxStepHeight : 0.0f;
	Sample.MaxDepenetration = Movement ? Movement->MaxDepenetrationWithGeometry : 0.0f;
	Sample.GravityScale = Movement ? Movement->GravityScale : 0.0f;

	// 位置を直接動かす処理は Velocity に出ないため、区間で切り分ける
	Sample.MoveOutsideTickSpeed = MovementSpikeOutsideTickSpeed;
	Sample.MoveModulesSpeed = MoveModulesSpeed;
	Sample.MovePushWindSpeed = MovePushWindSpeed;
	Sample.WindSpeed = SmoothedHorizontalWindVelocity.Size();
	Sample.bAnimRootMotion = Movement ? Movement->HasAnimRootMotion() : false;
	if ( const UAnimMontage* Montage = GetCurrentMontage() )
	{
		Sample.MontageName = Montage->GetName();
	}

	// 接線側が膨らめば斜面での移動量の伸び、法線側なら縦方向の位置補正と切り分けられる
	{
		const FVector Up = CurrentGravityUp;
		Sample.MoveUpSpeed = FVector::DotProduct( MovementSpikeActualMove, Up );
		Sample.MoveTangentSpeed = FVector::VectorPlaneProject( MovementSpikeActualMove, Up ).Size();
		Sample.VelUpSpeed = FVector::DotProduct( MovementSpikeVelIn, Up );
		Sample.VelTangentSpeed = FVector::VectorPlaneProject( MovementSpikeVelIn, Up ).Size();
	}

	Sample.bOverlappingGeometry = bPenetrating;
	Sample.PenetrationDepth = PenetrationDepth;
	Sample.PenetrationName = PenetrationName;

	// 面沿いの追従遅れ。開いていると速度を置く平面が実際の面から傾いて離脱成分・目減りが出る
	if ( Movement )
	{
		const FVector RawNormal = Movement->CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
		if ( !RawNormal.IsNearlyZero() )
		{
			Sample.GravityLagDeg = FMath::RadiansToDegrees(
				FMath::Acos( FMath::Clamp( FVector::DotProduct( CurrentGravityUp, RawNormal ), -1.0f, 1.0f ) ) );
		}
	}

	MovementSpikeLog.Insert( Sample, 0 );
	constexpr int32 MaxSamples = 8;
	if ( MovementSpikeLog.Num() > MaxSamples )
	{
		MovementSpikeLog.SetNum( MaxSamples );
	}
}

void ATidePlayerCharacter::UpdateSurfaceRideInputBasis( float DeltaTime )
{
	if ( !bIsSurfaceRiding )
	{
		SurfaceRideInputForward = FVector::ZeroVector;
		SurfaceRideInputDegenerateSin = 1.0f;
		return;
	}

	const FVector Up = CurrentGravityUp;
	const FVector CameraForward = GetControlRotation().Vector();

	// 投影した長さ＝カメラ前方と床法線のなす角の sin。カメラが面を正面から見る関係に近づくと 0 へ潰れ、
	// 正規化で向きが極端に敏感になる。入力方向が一瞬振れると AddMovementInput がそちらへ加速して
	// 「あらぬ方向へ急に移動してガクッ」となるため、前フレームの基準を持ち回す
	const FVector Projected = FVector::VectorPlaneProject( CameraForward, Up );
	SurfaceRideInputDegenerateSin = Projected.Size();

	const float DegenerateSin = PlayerParamData ? PlayerParamData->SurfaceRideInputDegenerateSin : 0.0f;

	FVector Target = FVector::ZeroVector;
	if ( SurfaceRideInputDegenerateSin >= DegenerateSin )
	{
		Target = Projected.GetSafeNormal();
	}
	else
	{
		// 持ち回し → カメラ上方向 → 機体前方の順にフォールバック（いずれも接平面へ投影して使う）
		Target = FVector::VectorPlaneProject( SurfaceRideInputForward, Up ).GetSafeNormal();
		if ( Target.IsNearlyZero() )
		{
			Target = FVector::VectorPlaneProject( FRotationMatrix( GetControlRotation() ).GetUnitAxis( EAxis::Z ), Up ).GetSafeNormal();
		}
		if ( Target.IsNearlyZero() )
		{
			Target = FVector::VectorPlaneProject( GetActorForwardVector(), Up ).GetSafeNormal();
		}
	}

	if ( Target.IsNearlyZero() ) return;	// 基準が作れないフレームは前回値を維持する

	// カメラ操作による基準の回転はこれより十分遅いので操作感は変わらず、
	// 縮退域の出入りで基準が飛ぶケースだけが均される（0 で即時）
	const FVector Current = FVector::VectorPlaneProject( SurfaceRideInputForward, Up ).GetSafeNormal();
	const float TurnSpeed = PlayerParamData ? PlayerParamData->SurfaceRideInputForwardTurnSpeedDeg : 0.0f;
	SurfaceRideInputForward = ( TurnSpeed > 0.0f && !Current.IsNearlyZero() )
		? FMath::VInterpNormalRotationTo( Current, Target, DeltaTime, TurnSpeed )
		: Target;
}

void ATidePlayerCharacter::GetInputBasis( FVector& OutForward, FVector& OutRight ) const
{
	const FRotator ControlRot = GetControlRotation();

	if ( !bIsSurfaceRiding )
	{
		const FRotationMatrix YawMat( FRotator( 0.0f, ControlRot.Yaw, 0.0f ) );
		OutForward = YawMat.GetUnitAxis( EAxis::X );
		OutRight = YawMat.GetUnitAxis( EAxis::Y );
		return;
	}

	// 面沿い中の基準は面の接平面。カメラの 3D 前方（Pitch 込み）を投影するので壁を正面から見ていれば
	// 前入力が「壁を登る」方向になる（Yaw だけだと垂直な壁で投影が潰れて登れない）
	const FVector Up = CurrentGravityUp;

	// 【検証・方式2】チューブ基準入力：前＝進行方向へ加速／左右＝筒の円周を回る
	if ( IsInSurfaceRideTubeRelativeZone() )
	{
		FVector Travel = FVector::VectorPlaneProject( GetVelocity(), Up ).GetSafeNormal();
		if ( Travel.IsNearlyZero() )
		{
			Travel = FVector::VectorPlaneProject( GetActorForwardVector(), Up ).GetSafeNormal();
		}
		if ( !Travel.IsNearlyZero() )
		{
			OutForward = Travel;
			OutRight = FVector::CrossProduct( Up, Travel ).GetSafeNormal();
			return;
		}
	}

	// 前方は UpdateSurfaceRideInputBasis が安定化した値を使い、まだ作られていない（ライド開始直後）
	// フレームだけその場で作る
	FVector Forward = FVector::VectorPlaneProject( SurfaceRideInputForward, Up ).GetSafeNormal();
	if ( Forward.IsNearlyZero() )
	{
		Forward = FVector::VectorPlaneProject( ControlRot.Vector(), Up ).GetSafeNormal();
	}
	// カメラが面の真正面を向いている縮退時はカメラ上方向で代用する
	if ( Forward.IsNearlyZero() )
	{
		Forward = FVector::VectorPlaneProject( FRotationMatrix( ControlRot ).GetUnitAxis( EAxis::Z ), Up ).GetSafeNormal();
	}
	if ( Forward.IsNearlyZero() )
	{
		Forward = FVector::VectorPlaneProject( GetActorForwardVector(), Up ).GetSafeNormal();
	}

	FVector Right;
	if ( IsInSurfaceRideScreenRelativeZone() )
	{
		// 【B案】画面基準：Up が反転しても「画面右＝右」のままで鏡像にならない
		Right = FVector::VectorPlaneProject( FRotationMatrix( ControlRot ).GetUnitAxis( EAxis::Y ), Up ).GetSafeNormal();
		if ( Right.IsNearlyZero() ) Right = FVector::CrossProduct( Up, Forward ).GetSafeNormal();
	}
	else
	{
		// 従来：面の上方向基準。天井側では画面上鏡像に見える
		Right = FVector::CrossProduct( Up, Forward ).GetSafeNormal();
	}

	// 【検証】天井側でのみ効く入力反転。左右反転は B案（画面基準）と併用すると逆に鏡像化するので二者択一
	if ( Up.Z < -KINDA_SMALL_NUMBER )
	{
		if ( IsInSurfaceRideInvertZone() ) Right = -Right;
		if ( IsInSurfaceRideInvertForwardZone() ) Forward = -Forward;
	}

	OutForward = Forward;
	OutRight = Right;
}

FVector ATidePlayerCharacter::GetControlRelativeInputDirection( const FVector2D& InRawInput ) const
{
	FVector Forward, Right;
	GetInputBasis( Forward, Right );
	return Forward * InRawInput.Y + Right * InRawInput.X;
}

void ATidePlayerCharacter::UpdateChargeDashLoopBlendSpace( int32 GearIndex, float DeltaTime )
{
	UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( GetMesh()->GetAnimInstance() );
	if ( !AnimInst ) return;

	AnimInst->bIsChargeDashBSPlaying = true;
	AnimInst->ChargeDashGearIndex = GearIndex;

	// ダッシュ中は移動がロック駆動で AddMovementInput を通さない（LastInputVector が 0）ため生入力から求める
	const FVector2D RawInput = GetRawMovementInput();
	float TargetDirection = 0.0f;
	if ( !RawInput.IsNearlyZero() )
	{
		const FVector WorldInput = GetControlRelativeInputDirection( RawInput ).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct( WorldInput, GetActorForwardVector() );
		const float RightDot = FVector::DotProduct( WorldInput, GetActorRightVector() );
		// BS の軸は前方半球（-90〜90）
		TargetDirection = FMath::Clamp( FMath::RadiansToDegrees( FMath::Atan2( RightDot, ForwardDot ) ), -90.0f, 90.0f );
	}

	const float InterpSpeed = PlayerParamData ? PlayerParamData->ChargeDashDirectionInterpSpeed : 10.0f;
	const FRotator CurrentRot( 0.0f, AnimInst->ChargeDashDirection, 0.0f );
	const FRotator TargetRot( 0.0f, TargetDirection, 0.0f );
	AnimInst->ChargeDashDirection = FMath::RInterpTo( CurrentRot, TargetRot, DeltaTime, InterpSpeed ).Yaw;
}

void ATidePlayerCharacter::StopChargeDashLoopBlendSpace()
{
	if ( UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>( GetMesh()->GetAnimInstance() ) )
	{
		AnimInst->bIsChargeDashBSPlaying = false;
	}
}

FName ATidePlayerCharacter::GetChargeAttackAnimTag() const
{
	if ( IsUsingChargeV2() )
	{
		return CachedChargeModuleV2 ? CachedChargeModuleV2->GetChargeAttackAnimTag() : NAME_None;
	}
	return CachedChargeModule ? CachedChargeModule->GetChargeAttackAnimTag() : NAME_None;
}

bool ATidePlayerCharacter::TryStartAirNormalDiveAttack()
{
	if ( IsDead() ) return false;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return false;
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		return CachedChargeModuleV2->TryStartAirNormalDiveAttack();
	}
	return false;
}

bool ATidePlayerCharacter::IsAirNormalDiveAttack() const
{
	return CachedChargeModuleV2 ? CachedChargeModuleV2->IsAirNormalDiveAttack() : false;
}

bool ATidePlayerCharacter::IsAirChargeAttackActionLocked() const
{
	return CachedChargeModuleV2 ? CachedChargeModuleV2->IsAirChargeAttackActionLocked() : false;
}

bool ATidePlayerCharacter::IsInGlideSession() const
{
	return CachedGlideModule ? CachedGlideModule->IsGliding() : false;
}

float ATidePlayerCharacter::GetGlideRemainingTime() const
{
	return CachedGlideModule ? CachedGlideModule->GetRemainingTime() : 0.0f;
}

bool ATidePlayerCharacter::RequestGlideAfterAirChargeDash()
{
	return CachedGlideModule ? CachedGlideModule->RequestGlideAfterAirChargeDash() : false;
}

int32 ATidePlayerCharacter::GetCurrentChargeComboIndex() const
{
	if ( !CachedChargeModuleV2 ) return 0;
	return CachedChargeModuleV2->GetCurrentChargeComboIndex();
}

int32 ATidePlayerCharacter::GetCurrentChargeGearIndex() const
{
	if ( !CachedChargeModuleV2 ) return 0;
	return CachedChargeModuleV2->GetCurrentChargeGearIndex();
}

void ATidePlayerCharacter::FellOutOfWorld( const UDamageType& DmgType )
{
	if ( FallRecoveryComponent )
	{
		FallRecoveryComponent->RequestRecovery();
	}
}

void ATidePlayerCharacter::OnMovementModeChanged( EMovementMode PrevMovementMode, uint8 PreviousCustomMode )
{
	Super::OnMovementModeChanged( PrevMovementMode, PreviousCustomMode );

	RefreshMovementParams();
}

void ATidePlayerCharacter::NotifyHit( UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit )
{
	Super::NotifyHit( MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit );

	if ( IsUsingChargeV2() )
	{
		if ( CachedChargeModuleV2 )
		{
			CachedChargeModuleV2->OnCharacterHit( Hit );
		}
	}
	else
	{
		if ( CachedChargeModule )
		{
			CachedChargeModule->OnCharacterHit( Hit );
		}
	}
}

void ATidePlayerCharacter::Landed( const FHitResult& Hit )
{
	Super::Landed( Hit );

	if ( CachedJumpModule )
	{
		CachedJumpModule->OnLanded();
	}
	// 滑空の後始末は JumpModule のあと
	if ( CachedGlideModule )
	{
		CachedGlideModule->OnLanded();
	}
	if ( CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->OnLanded();
	}
	if ( CachedFallModule )
	{
		CachedFallModule->OnLanded( Hit );
	}
	if ( CachedLaunchLockModule )
	{
		CachedLaunchLockModule->OnLanded();	// 頂点前に着地・天井衝突した場合の保険解除
	}

	RefreshMovementParams();
}

void ATidePlayerCharacter::OnModifyDamageInfo( FDamageInfo& OutDamageInfo, FGameplayTag AttackTypeTag, AActor* Target )
{
	bool bIsRebounded = true;

	// チャージ攻撃モンタージュ再生中はロックタイマーが切れていてもチャージ側へ流す
	// （通常攻撃側へフォールバックすると弱いノックバックになる）
	const bool bIsChargeAttackHit = CachedChargeModuleV2 &&
		( IsPlayingChargeAction() || CachedChargeModuleV2->IsPlayingChargeAttackMontage() );
	if ( bIsChargeAttackHit )
	{
		CachedChargeModuleV2->OnAttackHit( Target, bIsRebounded );
	}
	else if ( IsAttacking() && CachedAttackModule )
	{
		CachedAttackModule->OnAttackHit( Target, bIsRebounded );
	}

	OutDamageInfo.AttackTypeTag = AttackTypeTag;

	const int32 CurrentGearIndex = GetCurrentChargeGearIndex();

	switch ( CurrentGearIndex )
	{
	case 1:		OutDamageInfo.ChargeGearTag = TAG_Charge_Gear1;		break;
	case 2:		OutDamageInfo.ChargeGearTag = TAG_Charge_Gear2;		break;
	case 3: 	OutDamageInfo.ChargeGearTag = TAG_Charge_Gear3;		break;
	case 4: 	OutDamageInfo.ChargeGearTag = TAG_Charge_Gear4; 	break;
	default:	break;
	}

	if ( const UTideCharacterDataAsset* DataAsset = GetCharacterData() )
	{
		if ( DataAsset->AttackParameterTable )
		{
			static const FString ContextString = TEXT( "AttackParameterLookup" );
			TArray<FPlayerAttackParameterRow*> PlayerRows;
			DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( ContextString, PlayerRows );

			for ( FPlayerAttackParameterRow* Row : PlayerRows )
			{
				if ( Row &&
					( Row->AttackTypeTag == OutDamageInfo.AttackTypeTag ) &&
					( Row->GearLevel == CurrentGearIndex ) )
				{
					OutDamageInfo.BaseDamage = CalculateDamage( Row->DamageMultiplier );

					OutDamageInfo.StaggerDamage = Row->StaggerDamage;

					const ACharacter* TargetChar = Cast<ACharacter>(Target);
					const bool bTargetAirborne = TargetChar
						&& TargetChar->GetCharacterMovement()
						&& TargetChar->GetCharacterMovement()->IsFalling();
					OutDamageInfo.HitReactionTag = (bTargetAirborne && Row->EnemyAirReactionTag.IsValid())
						? Row->EnemyAirReactionTag
						: Row->EnemyReactionTag;

					OutDamageInfo.bUseHitStop = Row->bUseHitStop;
					OutDamageInfo.HitStopDuration = Row->HitStopDuration;
					OutDamageInfo.HitStopDilation = Row->HitStopDilation;

					// 保険。一閃ワイドカットのダメージは ED で一斉適用する設計なので、LP モンタージュに
					// 攻撃判定が残っていると通過のたびに再発火して「止まる」
					if ( OutDamageInfo.AttackTypeTag == TAG_AttackType_Player_GodActionSlash && IsGodSlashWideCutLoopPassthrough() )
					{
						OutDamageInfo.bUseHitStop = false;
					}

					constexpr int32 HitEffectType_None = 0;
					constexpr int32 HitEffectType_Small = 1;
					constexpr int32 HitEffectType_Medium = 2;
					constexpr int32 HitEffectType_Large = 3;

					FName HitEffectTag = NAME_None;
					switch ( Row->HitEffectType )
					{
					case HitEffectType_Small:	HitEffectTag = PlayerNiagaraTags::HIT_SMALL;	break;
					case HitEffectType_Medium:	HitEffectTag = PlayerNiagaraTags::HIT_MEDIUM;	break;
					case HitEffectType_Large:	HitEffectTag = PlayerNiagaraTags::HIT_LARGE;	break;
					case HitEffectType_None:
					default:
						break;
					}

					if ( Row->HitCameraShake )
					{
						if ( const APlayerController* PC = Cast<APlayerController>( GetController() ) )
						{
							if ( APlayerCameraManager* CamMgr = PC->PlayerCameraManager )
							{
								CamMgr->StartCameraShake( Row->HitCameraShake, Row->HitCameraShakeScale );
							}
						}
					}

					if ( HitEffectTag != NAME_None )
					{
						UNiagaraSystem* EffectSys = NiagaraSystemDataAsset ? NiagaraSystemDataAsset->GetNiagaraSystem( HitEffectTag ) : nullptr;
						if ( EffectSys )
						{
							if ( USkeletalMeshComponent* MeshComp = GetMesh() )
							{
								static const FName WeaponSocketName = TEXT( "Weapon_Blade" );

								const FTransform SocketTransform = MeshComp->GetSocketTransform( WeaponSocketName );

								constexpr float HitEffectLocalOffsetLength = 30.0f;
								const FVector LocalOffset( HitEffectLocalOffsetLength, 0.0f, 0.0f );

								const FVector SpawnLoc = SocketTransform.TransformPosition( LocalOffset );
								const FRotator SpawnRot = SocketTransform.Rotator();

								UNiagaraFunctionLibrary::SpawnSystemAtLocation( GetWorld(), EffectSys, SpawnLoc, SpawnRot );
							}
						}
					}

					break;
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// 敵側で方向・部位倍率が乗算されるため、最終ダメージは 9999 以上になる
	if ( UTideGameSettings::Get()->bDebugFlagPlayerAttack9999 )
	{
		OutDamageInfo.BaseDamage = 9999.0f;
	}
#endif

	// とどめ先読み。敵側 ReceiveDamage は SuperArmor 等で早期 return して致死判定に届かないことがあるので
	// プレイヤー側で先回りする（部位倍率で倒せるケースは敵側 OnDeliveredFinishingBlow が補完する）。
	// 「エネミー被ダメージなし」中は実際には死なないので先読みも止める
	bool bSkipLethalPreRead = false;
#if !UE_BUILD_SHIPPING
	bSkipLethalPreRead = UTideGameSettings::Get()->bDebugEnemyNoDamage;
#endif
	if ( const ATideCharacter* LethalTarget = bSkipLethalPreRead ? nullptr : Cast<ATideCharacter>( Target ) )
	{
		if ( const UStatusComponent* TargetStatus = LethalTarget->GetStatusComponent() )
		{
			const float TargetHP = TargetStatus->GetCurrentHP();
			if ( TargetHP > 0.0f && OutDamageInfo.BaseDamage >= TargetHP )
			{
				// OnAttackHit の直後＝慣性（Velocity）がまだヒット前の値なので、前進へ引き継ぐのに適した位置
				ResolveAttackHitBackOnLethalHit();

				// とどめではフィニッシュのスロー演出へ置き換わる。
				// AnimNotifyState_CommonAttack が bUseHitStop を見て双方へ適用するので false で両方止まる
				OutDamageInfo.bUseHitStop = false;
			}
		}
	}

	Super::OnModifyDamageInfo( OutDamageInfo, AttackTypeTag );
}

void ATidePlayerCharacter::StartWeaponTrail( FName NiagaraTag, float LifeTime, FName SocketName, const FVector& LocationOffset, const FRotator& RotationOffset )
{
	if ( NiagaraTag.IsNone() || !NiagaraSystemDataAsset ) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if ( !MeshComp ) return;

	UNiagaraSystem* TrailSys = NiagaraSystemDataAsset->GetNiagaraSystem( NiagaraTag );
	if ( !TrailSys ) return;

	// 攻撃→神技など連続切り替えに備えて貼り直す
	StopWeaponTrail();

	WeaponTrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		TrailSys,
		MeshComp,
		SocketName,
		LocationOffset,
		RotationOffset,
		EAttachLocation::KeepRelativeOffset,
		false,	// bAutoDestroy（停止はこちらで管理する）
		true	// bAutoActivate
	);

	if ( WeaponTrailVFX )
	{
		WeaponTrailVFX->SetFloatParameter( TEXT( "LifeTime" ), LifeTime );
	}
}

void ATidePlayerCharacter::StopWeaponTrail()
{
	if ( !WeaponTrailVFX ) return;

	// 放出だけ止め、残ったリボンは LifeTime ぶんフェードさせてから自動消滅させる
	WeaponTrailVFX->Deactivate();
	WeaponTrailVFX->SetAutoDestroy( true );
	WeaponTrailVFX = nullptr;
}

void ATidePlayerCharacter::UpdatePushPawn( float DeltaTime )
{
	auto* PushCapsuleComponent = GetCapsuleComponent();
	if ( !PushCapsuleComponent ) return;

	// 前フレームで正面衝突していたら安全圏へ巻き戻す
	if ( bIsPushingPawn )
	{
		SetActorLocation( PushTargetSafetyLocation, false );
		StopVelocity();
	}
	bIsPushingPawn = false;

	TArray<UPrimitiveComponent*> OverlappingComps;
	PushCapsuleComponent->GetOverlappingComponents( OverlappingComps );

	FVector TotalPushVector = FVector::ZeroVector;

	for ( UPrimitiveComponent* OtherComp : OverlappingComps )
	{
		if ( !OtherComp || OtherComp->GetOwner() == this ) continue;

		ATideCharacter* OtherCharacter = Cast<ATideCharacter>( OtherComp->GetOwner() );
		if ( !OtherCharacter ) continue;

		FVector MyLocation = PushCapsuleComponent->GetComponentLocation();
		FVector OtherLocation = OtherComp->GetComponentLocation();
		MyLocation.Z = 0.0f;
		OtherLocation.Z = 0.0f;

		FVector Direction = MyLocation - OtherLocation;
		float Distance = Direction.Size();

		constexpr float DefaultFallbackRadius = 30.0f;
		float OtherRadius = DefaultFallbackRadius;
		if ( UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>( OtherComp ) )
		{
			OtherRadius = OtherCapsule->GetScaledCapsuleRadius();
		}

		const float OptimalDistance = PushCapsuleComponent->GetScaledCapsuleRadius() + OtherRadius;

		if ( Distance >= OptimalDistance || Distance <= KINDA_SMALL_NUMBER )
		{
			continue;
		}

		Direction.Normalize();

		float CurrentSpeed2D = 0.0f;
		if ( UCharacterMovementComponent* MyMovement = GetCharacterMovement() )
		{
			CurrentSpeed2D = MyMovement->Velocity.Size2D();
		}

		constexpr float PushActionVelocityThreshold = 100.0f;
		constexpr float PushBackBuffer = 0.5f;

		// ビタ止めロックはアクション実行中のみ。溜め中（慣性移動中）は通常コリジョンのように滑らせたい
		bool bIsChargeAction = IsPlayingChargeAction();
		if ( PlayerParamData && PlayerParamData->bEnableChargeDashThrough )
		{
			bIsChargeAction = IsPlayingChargeAttack();
		}

		if ( bIsChargeAction && ( CurrentSpeed2D > PushActionVelocityThreshold || bIsPushingPawn ) && PlayerParamData )
		{
			const FVector ForwardDir = GetActorForwardVector();
			const float DotResult = FVector::DotProduct( ForwardDir, Direction );
			const float StopThreshold = PlayerParamData->ChargeMoveStopThreshold;

			if ( DotResult < StopThreshold )
			{
				// すでに敵から離れる方向へ動いている（ヒットバック等）なら止めない
				if ( UCharacterMovementComponent* MyMovement = GetCharacterMovement() )
				{
					const FVector VelocityDir2D = MyMovement->Velocity.GetSafeNormal2D();
					if ( FVector::DotProduct( VelocityDir2D, Direction ) > 0.0f )
					{
						continue;
					}
				}

				// 敵へのリバウンドは重さ判定が未実装のため行わない（壁へのリバウンドは V2 の OnCharacterHit が担う）
				StopVelocity();
				bIsPushingPawn = true;
				continue;
			}
		}

		const float PenetrationDepth = OptimalDistance - Distance;

		constexpr float DefaultMass = 100.0f;
		float MyMass = DefaultMass;
		if ( UCharacterMovementComponent* MyMovement = GetCharacterMovement() ) { MyMass = MyMovement->Mass; }

		float OtherMass = DefaultMass;
		if ( UCharacterMovementComponent* OtherMovement = OtherCharacter->GetCharacterMovement() ) { OtherMass = OtherMovement->Mass; }

		const float WeightRatio = OtherMass / ( MyMass + OtherMass );

		// めり込み量を蓄積させず 1 フレームで解消する
		const float ResolveDistance = PenetrationDepth * WeightRatio;

		TotalPushVector += Direction * ResolveDistance;
	}

	if ( !TotalPushVector.IsNearlyZero() )
	{
		AddActorWorldOffset( TotalPushVector, false );

		// めり込み方向へ向かう速度を相殺して壁滑りにする
		if ( UCharacterMovementComponent* MyMovement = GetCharacterMovement() )
		{
			const FVector PushNormal = TotalPushVector.GetSafeNormal();
			const float VelocityDot = FVector::DotProduct( MyMovement->Velocity, PushNormal );

			if ( VelocityDot < 0.0f )
			{
				MyMovement->Velocity -= PushNormal * VelocityDot;
			}
		}
	}
	else
	{
		// めり込んでいないフレームの座標だけを安全圏として保存する
		PushTargetSafetyLocation = GetActorLocation();
	}
}

UAnimMontage* ATidePlayerCharacter::GetAnimMontage( const FName& MontageName ) const
{
	if ( !AnimMontageDataAsset ) return nullptr;
	return AnimMontageDataAsset->GetAnimMontage( MontageName );
}

float ATidePlayerCharacter::PlayAnimMontage( const FName& MontageName, float InPlayRate, FName StartSectionName )
{
	if ( !AnimMontageDataAsset ) return 0.0f;
	if ( UAnimMontage* Montage = GetAnimMontage( MontageName ) )
	{
		float PlayRate = 1.0f;
		if ( Montage->RateScale > 0.0f )	PlayRate = Montage->RateScale;
		return PlayAnimMontage( Montage, InPlayRate, StartSectionName ) / PlayRate;
	}
	return 0.0f;
}

float ATidePlayerCharacter::PlayAnimMontage( class UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName )
{
	return Super::PlayAnimMontage( AnimMontage, InPlayRate, StartSectionName );
}

void ATidePlayerCharacter::StopAnimMontage( class UAnimMontage* AnimMontage )
{
	Super::StopAnimMontage( AnimMontage );
}

void ATidePlayerCharacter::StopAnimMontage( float BlendOutTime, UAnimMontage* AnimMontage )
{
	if ( UAnimInstance* AnimInst = GetMesh()->GetAnimInstance() )
	{
		// 省略時は現在再生中のモンタージュを止める（ブレンド時間だけ指定したい用途）
		UAnimMontage* MontageToStop = AnimMontage ? AnimMontage : GetCurrentMontage();
		if ( MontageToStop )
		{
			AnimInst->Montage_Stop( BlendOutTime, MontageToStop );
		}
	}
}

// モンタージュ終了時のステートタグ掃除は AnimNotifyState 側が持つため、ここでは何もしない（バインド先として残す）
void ATidePlayerCharacter::OnMontageEnded( UAnimMontage* Montage, bool bInterrupted )
{
}

void ATidePlayerCharacter::OnMontageBlendingOut( UAnimMontage* Montage, bool bInterrupted )
{
}

void ATidePlayerCharacter::OnStatusDeath()
{
#if !UE_BUILD_SHIPPING
	if ( UTideGameSettings::Get()->bDebugFlagNoDeath ) return;
#endif

	ExecutePlayerDeath();

	if ( !RestartComponent ) return;

	// 死亡演出を見せるため暗転開始を遅らせる
	const float RestartDelay = PlayerParamData ? PlayerParamData->PlayerRestartDelay : 0.0f;
	if ( RestartDelay > 0.0f )
	{
		GetWorldTimerManager().SetTimer( RestartDelayTimerHandle,
			FTimerDelegate::CreateWeakLambda( this, [this]()
			{
				if ( RestartComponent )
				{
					RestartComponent->RequestRestart();
				}
			} ),
			RestartDelay, false );
	}
	else
	{
		RestartComponent->RequestRestart();
	}
}

void ATidePlayerCharacter::OnStatusRevive()
{
	ExecutePlayerRevive();
}

void ATidePlayerCharacter::ExecutePlayerDeath()
{
	if ( bIsDead ) return;

	bIsDead = true;

	CancelAllActions();
	StopVelocity();

	PlayAnimMontage( PlayerAnimTags::DIE );
}

void ATidePlayerCharacter::ExecutePlayerRevive()
{
	if ( !bIsDead ) return;
	bIsDead = false;
	StopAnimMontage( 0.0f );

	// 死亡時に R2 を握ったままだと押下ラッチが死亡をまたいで残り、復帰した次フレームに勝手に再チャージが始まる
	if ( CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->ResetChargeInputOnRevive();
	}

	// 斜面で死亡した DIE モーションの root motion で付いた傾きがリスタート後も残る。テレポートで向き直しても
	// 残留 root motion が次フレームの移動更新で回転を上書きするため、姿勢を戻したうえで root motion も畳む
	const FRotator UprightRotation( 0.0f, GetActorRotation().Yaw, 0.0f );
	SetActorRotation( UprightRotation );
	if ( UCharacterMovementComponent* Movement = GetCharacterMovement() )
	{
		Movement->RootMotionParams.Clear();
	}

	// 暗転中のテレポート後なので、カメラを補間させず即座に正面へ戻す（消費は ThirdPersonExCameraMode 側）
	RequestCameraReset();
}

EDamageResult ATidePlayerCharacter::ReceiveDamage( const FDamageInfo& DamageInfo )
{
	if ( IsDead() ) return EDamageResult::Immune;

	if ( HasStateTag( TAG_State_Player_Invincible_Dodge) || UTideGameSettings::Get()->bDebugFlagAutoDodgeSuccess )
	{
		if ( CachedDodgeModule ) CachedDodgeModule->OnDodgeSuccess();
		return EDamageResult::Evaded;
	}

	// ガードブレーキ中は1回だけ耐える。2回目以降は通常の被弾処理でガードブレーキごとキャンセルされる
	if ( CachedChargeModuleV2 && CachedChargeModuleV2->TryConsumeGuardBrakeBlock() )
	{
		// ガード成功が分かるよう、通常被弾と同じヒットストップを自分と攻撃側の両方へ掛ける
		if ( DamageInfo.bUseHitStop )
		{
			HitStopUtil::ApplyHitStop( this, DamageInfo.HitStopDuration, DamageInfo.HitStopDilation );
			if ( AActor* Attacker = DamageInfo.Instigator.Get() )
			{
				HitStopUtil::ApplyHitStop( Attacker, DamageInfo.HitStopDuration, DamageInfo.HitStopDilation );
			}
		}
		return EDamageResult::Evaded;
	}

	FDamageInfo ModifiedDamageInfo = DamageInfo;

	float CachedDamage = ModifiedDamageInfo.BaseDamage;
	if ( UTideGameSettings::Get()->bDebugFlagNoDamage )
	{
		ModifiedDamageInfo.BaseDamage = 0;
	}

	EDamageResult Result = Super::ReceiveDamage( ModifiedDamageInfo );

	// 回避・ガード成立・死亡中は手前で return 済み。分類はデバッグで 0 にする前の CachedDamage で行う
	PlayHitCameraShake( CachedDamage );

	// DoT は毎刻み加算を避けるため対象外。エネルギー玉の発生源は被弾箇所
	if ( !DamageInfo.bIsDamageOverTime && PlayerParamData )
	{
		FVector OrbStart = DamageInfo.HitResult.ImpactPoint;
		if ( OrbStart.IsNearlyZero() )
		{
			OrbStart = GetActorLocation();
		}
		AddGodActionGauge( PlayerParamData->GodActionGaugeGainOnDamaged, OrbStart );
	}

	// Super::ReceiveDamage 内で DIE モンタージュが同期的に再生されるため、死亡時にリアクションを出すと上書きになる。
	// DoT も毎刻みののけぞり連発を避けて出さない
	if ( CachedHitReactionModule && !IsDead() && !DamageInfo.bIsDamageOverTime )
	{
		FGameplayTag ReactionTag = DamageInfo.HitReactionTag;

		// 【暫定】敵の近接攻撃はまだ HitReactionTag を送らないためダメージ量で補完する。
		// 敵側のタグ対応が入り次第このブロックは撤去する
		if ( !ReactionTag.IsValid() )
		{
			ReactionTag = ( CachedDamage >= 10 ) ? TAG_HitReaction_Blowoff_M : TAG_HitReaction_Knockback_S;
		}

		if ( UTideGameSettings::Get()->bDebugFlagSuperArmor )
		{
			ReactionTag = TAG_HitReaction_NoReaction;
		}

		CachedHitReactionModule->ProcessHit( ReactionTag, DamageInfo.Instigator.Get() );
	}

	return Result;
}

bool ATidePlayerCharacter::CanBeDamaged() const
{
	if ( IsInvincible() ) return false;
	return Super::CanBeDamaged();
}

void ATidePlayerCharacter::PlayHitCameraShake( float DamageAmount )
{
	if ( !PlayerParamData || !PlayerParamData->HitCameraShake ) return;

	float Scale = PlayerParamData->HitCameraShakeScaleSmall;
	if ( DamageAmount >= PlayerParamData->HitCameraShakeLargeThreshold )
	{
		Scale = PlayerParamData->HitCameraShakeScaleLarge;
	}
	else if ( DamageAmount >= PlayerParamData->HitCameraShakeMediumThreshold )
	{
		Scale = PlayerParamData->HitCameraShakeScaleMedium;
	}

	if ( const APlayerController* PC = Cast<APlayerController>( GetController() ) )
	{
		if ( APlayerCameraManager* CamMgr = PC->PlayerCameraManager )
		{
			CamMgr->StartCameraShake( PlayerParamData->HitCameraShake, Scale );
		}
	}
}

// --- IWindAffectable ---

void ATidePlayerCharacter::OnWindEnter( const FWindInfluence& Wind )
{
	++WindSourceCount;
	CurrentWindJumpBoost = Wind.JumpBoostMultiplier;
	CurrentHorizontalWindVelocity = Wind.HorizontalWindVelocity;

	// 倍率の反映後に呼ぶ（ジャンプ側が GetWindJumpBoostMultiplier() を読む）
	TryForceWindJumpOnEnter();
}

void ATidePlayerCharacter::TryForceWindJumpOnEnter()
{
	if ( !PlayerParamData || !PlayerParamData->bForceWindJumpOnTornadoEnter ) return;

	// 強化倍率を配る風源（＝竜巻）だけ。WindZone の向かい風は倍率 1.0 なので反応しない
	if ( GetWindJumpBoostMultiplier() <= 1.0f ) return;

	if ( IsDead() ) return;
	if ( HasStateTag( TAG_State_Common_Disable ) ) return;

	// どんな状態で入っても同じ竜巻ジャンプにする。進行中のアクションが速度・モーションを握ったままだと
	// 「触れたときのアクションのまま」飛ぶので、主導権を移してから ForceJump する
	if ( IsUsingChargeV2() && CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->CancelChargeActionForWindJump();
	}
	if ( CachedGlideModule )
	{
		CachedGlideModule->CancelForWindJump();
	}

	ForceJump( false );
}

void ATidePlayerCharacter::OnWindTick( const FWindInfluence& Wind, float DeltaTime )
{
	// PL は速度オーバーライドで巻き上げない。水平風の押し出しも Tick の UpdateHorizontalWindPush() が
	// 補間済み速度から行うので、ここは目標値の更新だけに留める
	CurrentWindJumpBoost = Wind.JumpBoostMultiplier;
	CurrentHorizontalWindVelocity = Wind.HorizontalWindVelocity;

	// 中心への水平引き込み。Velocity には混ぜず位置オフセットで加える
	if ( Wind.bPullAffectsPlayer && Wind.PullSpeed > 0.0f && DeltaTime > 0.0f )
	{
		FVector ToCenter = Wind.Center - GetActorLocation();
		ToCenter.Z = 0.0f;
		const float Distance = ToCenter.Size();
		// 残り距離で頭打ちにし、中心を通り越して毎フレーム震えるのを防ぐ
		const float Step = FMath::Min( Wind.PullSpeed * DeltaTime, Distance );
		if ( Step > 0.0f )
		{
			const FVector PullOffset = ToCenter / Distance * Step;
			AddActorWorldOffset( PullOffset, /*bSweep=*/true );
		}
	}
}

void ATidePlayerCharacter::UpdateHorizontalWindPush( float DeltaTime )
{
	if ( DeltaTime <= 0.0f ) return;

	// 風の急な出入り（立ち上がり・減衰）を緩和する。VInterpTo は目標に十分近づくとスナップするので、
	// 風から抜けた後は確実にゼロへ収束する（補間速度 0 なら即時追従）
	const float InterpSpeed = PlayerParamData ? PlayerParamData->WindPushInterpSpeed : 0.0f;
	SmoothedHorizontalWindVelocity = ( InterpSpeed > 0.0f )
		? FMath::VInterpTo( SmoothedHorizontalWindVelocity, CurrentHorizontalWindVelocity, DeltaTime, InterpSpeed )
		: CurrentHorizontalWindVelocity;

	if ( SmoothedHorizontalWindVelocity.IsNearlyZero() ) return;

	// Velocity には混ぜず位置オフセットをコリジョン込み（bSweep）で加える。ダッシュの Velocity 上書きに
	// 消されず、Velocity 向きを見るターン検出も誤爆しない
	const float Scale = GetWindPushScale( SmoothedHorizontalWindVelocity );
	if ( Scale > 0.0f )
	{
		const FVector Offset = SmoothedHorizontalWindVelocity * Scale * DeltaTime;
		AddActorWorldOffset( Offset, /*bSweep=*/true );
	}
}

float ATidePlayerCharacter::GetWindPushScale( const FVector& WindDirection ) const
{
	if ( !PlayerParamData ) return 1.0f;

	// 優先度は向かい風／追い風とも共通：ブーストダッシュ＞突風ダッシュ＞チャージダッシュ＞通常
	float HeadwindScale;
	float TailwindScale;
	if ( IsBoostDashing() )
	{
		HeadwindScale = PlayerParamData->WindPushScaleBoostDash;
		TailwindScale = PlayerParamData->WindPushScaleTailwindBoostDash;
	}
	else if ( IsGustBuffedChargeActionActive() )
	{
		HeadwindScale = PlayerParamData->WindPushScaleGustDash;
		TailwindScale = PlayerParamData->WindPushScaleTailwindGustDash;
	}
	else if ( IsPlayingChargeDash() )
	{
		HeadwindScale = PlayerParamData->WindPushScaleChargeDash;
		TailwindScale = PlayerParamData->WindPushScaleTailwindChargeDash;
	}
	else
	{
		HeadwindScale = PlayerParamData->WindPushScaleNormal;
		TailwindScale = PlayerParamData->WindPushScaleTailwindNormal;
	}

	// 風向きの概念がない呼び出しでは向かい風スケールを使う
	if ( WindDirection.IsNearlyZero() ) return HeadwindScale;

	// なす角のコサイン（-1=真正面の向かい風 〜 +1=真後ろの追い風）でブレンドする。
	// 90 度での二値切り替えと違い全周にわたって連続的に変化する
	const float Alignment = FVector::DotProduct( WindDirection.GetSafeNormal(), GetActorForwardVector() );
	const float BlendRatio = ( Alignment + 1.0f ) * 0.5f;
	return FMath::Lerp( HeadwindScale, TailwindScale, BlendRatio );
}

float ATidePlayerCharacter::GetGroundPullScale() const
{
	if ( !PlayerParamData ) return 1.0f;

	// 状態優先度は GetWindPushScale() と共通。風向きの概念はないので専用倍率を状態別に返すだけ
	if ( IsBoostDashing() )                     return PlayerParamData->GroundPullScaleBoostDash;
	if ( IsGustBuffedChargeActionActive() )     return PlayerParamData->GroundPullScaleGustDash;
	if ( IsPlayingChargeDash() )                return PlayerParamData->GroundPullScaleChargeDash;
	return PlayerParamData->GroundPullScaleNormal;
}

void ATidePlayerCharacter::OnWindExit()
{
	if ( WindSourceCount <= 0 ) return;
	--WindSourceCount;
	if ( WindSourceCount == 0 )
	{
		CurrentWindJumpBoost = 1.0f;
		CurrentHorizontalWindVelocity = FVector::ZeroVector;
	}
}

// --- IGroundPullAffectable ---

void ATidePlayerCharacter::OnGroundPullTick( const FGroundPullInfluence& Pull, float DeltaTime )
{
	// Wind の水平風と同じ流儀で、Velocity には混ぜず位置オフセットを bSweep で加える
	if ( Pull.PullVelocity.IsNearlyZero() || DeltaTime <= 0.0f ) return;

	const float Scale = GetGroundPullScale();
	if ( Scale <= 0.0f ) return;

	const FVector Offset = Pull.PullVelocity * Scale * DeltaTime;
	AddActorWorldOffset( Offset, /*bSweep=*/true );
}

void ATidePlayerCharacter::UpdateUI()
{
	auto* CurrentPC = UGameplayStatics::GetPlayerController( GetWorld(), 0 );
	if ( GetController() != CurrentPC )
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// 即消しではなく out アニメで引っ込めるため、早期 return せず bWantVisible として渡す
	// （消え切るまで Draw を呼び続ける必要がある）
	const bool bHudWantVisible = !TideHudAnim::IsHudSuppressed( GetWorld() );

	// 暫定 HUD。正式 UI 実装時は Placeholder ごと削除し、この呼び出しを外す
	if ( StatusComponent )
	{
		PlayerHudPlaceholder::Draw( GetWorld(), 1, StatusComponent->GetCurrentHP(), StatusComponent->GetMaxHP(), bHudWantVisible );
	}

	if ( CachedGodActionModule )
	{
		GodActionHudPlaceholder::FGodActionHudData HudData;
		HudData.GaugeRate = CachedGodActionModule->GetGaugeRate();
		HudData.bGaugeFull = CachedGodActionModule->IsGaugeFull();
		HudData.bLockingOn = CachedGodActionModule->IsLockingOn();
		HudData.bSlashing = CachedGodActionModule->IsSlashing();
		HudData.LockedCount = CachedGodActionModule->GetLockedTargetCount();
		HudData.MaxLockOnCount = CachedGodActionModule->GetMaxLockOnCount();
		CachedGodActionModule->GetLockedTargetLocations( HudData.LockedTargetWorldLocations );

		// 共有の神鳥が出払っている間は鳥を使う神技（戯/導）を選べないのでグレーアウトさせる
		HudData.bBirdAway = IsGodFrolicActive() || IsGodGuidanceActive();

		if ( CachedGodActionModule->IsGodBirdModeEnabled() )
		{
			HudData.bGodArtSelecting = CachedGodActionModule->IsGodArtSelecting();
			HudData.SelectedArt = CachedGodActionModule->GetSelectedArtIndex();
			HudData.bFaceButtonSelect = CachedGodActionModule->IsFaceButtonSelectEnabled();
			// 「導き」選択中はロックオン枠を出さない
			if ( HudData.bGodArtSelecting && HudData.SelectedArt == 1 )
			{
				HudData.bLockingOn = false;
			}
		}

		// モジュールが所有するエネルギー玉の飛行状態を HUD 描画用へ移し替える
			TArray<UGodActionPlayerModule::FGodGaugeOrbView> OrbViews;
			CachedGodActionModule->GetGaugeOrbViews( OrbViews );
			HudData.Orbs.Reserve( OrbViews.Num() );
			for ( const UGodActionPlayerModule::FGodGaugeOrbView& V : OrbViews )
			{
				GodActionHudPlaceholder::FGodActionHudData::FOrbView O;
				O.StartWorldLoc = V.StartWorldLoc;
				O.Progress = V.Progress;
				O.Lateral = V.Lateral;
				O.RadiusScale = V.RadiusScale;
				O.AlphaScale = V.AlphaScale;
				HudData.Orbs.Add( O );
			}
			if ( PlayerParamData )
			{
				HudData.OrbRadius = PlayerParamData->GodGaugeOrbRadius;
				HudData.OrbCurveStrength = PlayerParamData->GodGaugeOrbCurveStrength;
				HudData.OrbFlashTime = PlayerParamData->GodGaugeFlashTime;
				HudData.OrbAlpha = PlayerParamData->GodGaugeOrbAlpha;
			}

			GodActionHudPlaceholder::Draw( GetWorld(), CurrentPC, HudData, bHudWantVisible );
	}
#endif
}

#if !UE_BUILD_SHIPPING

#include "DrawDebugHelpers.h"
void ATidePlayerCharacter::DrawDebugCoordinate()
{
	if ( !UTideGameSettings::Get()->bDebugFlagDrawTransformCoordinate && !UTideGameSettings::Get()->bDebugFlagDrawRootCoordinate ) return;

	constexpr float AxisLength = 100.0f;
	constexpr bool bPersistentLines = false;
	constexpr float LifeTime = -1.0f;	// 1フレームで消去
	constexpr uint8 DepthPriority = 0;
	constexpr float Thickness = 3.0f;

	if ( UTideGameSettings::Get()->bDebugFlagDrawTransformCoordinate )
	{
		DrawDebugCoordinateSystem( GetWorld(), GetActorLocation(), GetActorRotation(), AxisLength, bPersistentLines, LifeTime, DepthPriority, Thickness );
	}
	if ( UTideGameSettings::Get()->bDebugFlagDrawRootCoordinate )
	{
		if ( USkeletalMeshComponent* MeshComp = GetMesh() )
		{
			const FTransform BoneTransform = MeshComp->GetSocketTransform( TEXT( "root" ), RTS_World );
			DrawDebugCoordinateSystem( GetWorld(), BoneTransform.GetLocation(), BoneTransform.Rotator(), AxisLength, bPersistentLines, LifeTime, DepthPriority, Thickness );
		}
	}
}

#endif

void ATidePlayerCharacter::ResolveAttackHitBackOnLethalHit()
{
	if ( !PlayerParamData || !PlayerParamData->bEnableFinisherBreakthrough )
	{
		CancelAttackHitBack();
		return;
	}

	// OnAttackHit の後退はまだ PendingLaunchVelocity／ヒットバック枠に載っているだけで Velocity には出ていない。
	// したがってこの時点の速度＝ヒット直前の慣性をそのまま前進へ引き継げる
	float PreHitSpeed2D = 0.0f;
	if ( const UCharacterMovementComponent* Move = GetCharacterMovement() )
	{
		PreHitSpeed2D = Move->Velocity.Size2D();
	}

	// 対象詰め（TimedApproach）は SetActorLocation で動かし毎フレーム Velocity を 0 にするため、
	// 詰めの勢いは Velocity 計測に出ない。詰め側が実移動量から計測した速度を拾い直す
	if ( CachedChargeModuleV2 )
	{
		PreHitSpeed2D = FMath::Max( PreHitSpeed2D, CachedChargeModuleV2->GetChargeAttackApproachSpeed2D() );
	}

	float ThroughSpeed = FMath::Max(
		PreHitSpeed2D * PlayerParamData->FinisherBreakthroughInheritRate,
		PlayerParamData->FinisherBreakthroughMinSpeed );

	// 詰めはイーズインで終盤に一気に寄せるため実測速度が跳ねることがある
	if ( PlayerParamData->FinisherBreakthroughMaxSpeed > 0.0f )
	{
		ThroughSpeed = FMath::Min( ThroughSpeed, PlayerParamData->FinisherBreakthroughMaxSpeed );
	}

	const FVector ThroughDir = GetActorForwardVector().GetSafeNormal2D();

	// 各モジュールは「自分がヒットバックを張ったときだけ」引き受ける（前進の二重適用防止）
	if ( CachedAttackModule )
	{
		CachedAttackModule->ApplyBreakthroughMove( ThroughDir, ThroughSpeed );
	}
	if ( CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->ApplyBreakthroughMove( ThroughDir, ThroughSpeed );
	}
}

void ATidePlayerCharacter::CancelAttackHitBack()
{
	// 致死・破壊検知はヒットバックがセットされた同フレームの ReceiveDamage 内で行われるため、
	// ここで取り消せば実移動が始まる前に消えて見た目に出ない
	if ( CachedAttackModule )
	{
		CachedAttackModule->CancelHitBack();
	}
	if ( CachedChargeModuleV2 )
	{
		CachedChargeModuleV2->CancelHitBack();
	}
}

void ATidePlayerCharacter::OnDeliveredFinishingBlow( AActor* Enemy, const FDamageInfo& DamageInfo )
{
	ResolveAttackHitBackOnLethalHit();

	if ( CachedFinisherModule )
	{
		CachedFinisherModule->TriggerFinisher();
	}
}

void ATidePlayerCharacter::OnAttackHitConfirmed( const FDamageInfo& DamageInfo )
{
	if ( CachedGodActionModule )
	{
		CachedGodActionModule->OnDealtDamage( DamageInfo );
	}

#if !UE_BUILD_SHIPPING
	const FString TagFull = DamageInfo.AttackTypeTag.ToString();
	static const FString Prefix = TEXT("AttackType.Player.");
	const FString TagShort = TagFull.StartsWith( Prefix ) ? TagFull.RightChop( Prefix.Len() ) : TagFull;

	PlayerWindow::AttackLog.AddLog( FString::Printf(
		TEXT("[HIT] %s | Dmg=%.1f | Reaction=%s"),
		*TagShort,
		DamageInfo.BaseDamage,
		*DamageInfo.HitReactionTag.ToString()
	));
#endif
}

void ATidePlayerCharacter::StopAllMovementAndInputs()
{
	CancelAllActions();

	SetDodgeInputHeld( false );
	SetJumpInputHeld( false );
	CachedMovementInput = FVector2D::ZeroVector;

	if ( class UCharacterMovementComponent* MovementComp = GetCharacterMovement() )
	{
		MovementComp->StopMovementImmediately();
		MovementComp->ClearAccumulatedForces();
	}

	ConsumeMovementInputVector();
}

// 現在どこからも呼ばれていない（ImGui のデバッグ被弾は別経路）。使うなら TakeDamage をここで呼ぶ
void ATidePlayerCharacter::DebugDamage( float DamageAmount )
{
}

#if !UE_BUILD_SHIPPING
void ATidePlayerCharacter::DrawAttackDebugImGui()
{
	if ( !CachedAttackModule ) return;
	CachedAttackModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawDodgeDebugImGui()
{
	if ( !CachedDodgeModule ) return;
	CachedDodgeModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawLockOnDebugImGui()
{
	if ( !CachedLockOnModule ) return;
	CachedLockOnModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawFallDebugImGui()
{
	if ( !CachedFallModule ) return;
	CachedFallModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawChargeActionDebugImGui()
{
	if ( !CachedChargeModuleV2 ) return;
	CachedChargeModuleV2->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawGlideDebugImGui()
{
	if ( !CachedGlideModule ) return;
	CachedGlideModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawGodActionDebugImGui()
{
	if ( !CachedGodActionModule ) return;
	CachedGodActionModule->DrawDebugImGui();
}

void ATidePlayerCharacter::DrawSlidePassiveDebugImGui()
{
	if ( !CachedSlidePassiveModule ) return;
	CachedSlidePassiveModule->DrawDebugImGui();
}
#endif

void ATidePlayerCharacter::DrawDebugHomingArea( const FVector& InStartLoc, const FVector& InDefaultDir, const FVector& InResultDir, ULockOnTargetComponent* InTargetComp, float InDistance, float InAngle, float InDuration, FColor InAreaColor, float InMaxHeightDiff ) const
{
	const UWorld* World = GetWorld();
	if ( !World ) return;

	constexpr float LineThickness = 2.0f;
	constexpr float TargetLineThickness = 4.0f;
	constexpr int32 ArcSegments = 16;
	constexpr float SphereRadius = 16.0f;
	constexpr int32 SphereSegments = 12;

	const FColor AreaColor = InAreaColor;
	const FColor DefaultDirColor = FColor::Blue;
	const FColor TargetColor = FColor::Red;

	DrawDebugLine( World, InStartLoc, InStartLoc + ( InDefaultDir * InDistance ), DefaultDirColor, false, InDuration, 0, LineThickness );

	const float AngleStep = InAngle / static_cast< float >( ArcSegments );
	auto DrawFan = [&]( float ZOffset, FColor Color )
	{
		const FVector Apex = InStartLoc + FVector( 0.0f, 0.0f, ZOffset );

		const FVector LeftDir = InDefaultDir.RotateAngleAxis( -InAngle * 0.5f, FVector::UpVector );
		const FVector RightDir = InDefaultDir.RotateAngleAxis( InAngle * 0.5f, FVector::UpVector );
		DrawDebugLine( World, Apex, Apex + ( LeftDir * InDistance ), Color, false, InDuration, 0, LineThickness );
		DrawDebugLine( World, Apex, Apex + ( RightDir * InDistance ), Color, false, InDuration, 0, LineThickness );

		FVector PrevArcPoint = Apex + ( LeftDir * InDistance );
		for ( int32 i = 1; i <= ArcSegments; ++i )
		{
			const FVector CurrentDir = InDefaultDir.RotateAngleAxis( -InAngle * 0.5f + ( AngleStep * i ), FVector::UpVector );
			const FVector CurrentArcPoint = Apex + ( CurrentDir * InDistance );

			DrawDebugLine( World, PrevArcPoint, CurrentArcPoint, Color, false, InDuration, 0, LineThickness );
			PrevArcPoint = CurrentArcPoint;
		}
	};

	DrawFan( 0.0f, AreaColor );

	// 高さ差ゲートの上限。この帯の外にいる敵は吸着対象にならない
	if ( InMaxHeightDiff > 0.0f )
	{
		const FColor BandColor( static_cast< uint8 >( AreaColor.R / 2 ), static_cast< uint8 >( AreaColor.G / 2 ), static_cast< uint8 >( AreaColor.B / 2 ) );
		DrawFan( InMaxHeightDiff, BandColor );
		DrawFan( -InMaxHeightDiff, BandColor );
	}

	if ( InTargetComp != nullptr )
	{
		const FVector TargetLoc = InTargetComp->GetTargetLocation();
		DrawDebugLine( World, InStartLoc, TargetLoc, TargetColor, false, InDuration, 0, TargetLineThickness );
		DrawDebugSphere( World, TargetLoc, SphereRadius, SphereSegments, TargetColor, false, InDuration, 0, LineThickness );
	}
}
