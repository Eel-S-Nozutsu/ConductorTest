// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ChargeActionPlayerModule.generated.h"

enum class EChargeActionType : uint8
{
	None,
	Dash,
	Attack,
	Jump
};

UCLASS()
class UChargeActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void BeginCharge();
	void ReleaseCharge();

	void CancelCharge( bool bRestoreDash = true );

	bool IsCharging() const { return bIsCharging; }
	bool IsPlayingChargeAction() const;
	bool IsPlayingChargeDash() const { return CurrentChargeActionType == EChargeActionType::Dash; }
	bool IsPlayingChargeJump() const { return CurrentChargeActionType == EChargeActionType::Jump; }
	bool IsPlayingChargeAttack() const { return CurrentChargeActionType == EChargeActionType::Attack; }

	bool RequestChargeAttack();
	bool RequestChargeJump();

	void OnCharacterHit( const FHitResult& Hit );

	int32 GetCurrentChargeComboIndex() const { return CurrentChargeComboIndex; }
	FName GetChargeAttackAnimTag() const;

private:
	bool UpdateInputCommands();
	void UpdateComboReset();
	void UpdateChargeJumpLanding();
	void UpdateChargeDashMontageState();
	void UpdateChargeDashCameraPop( float DeltaTime );
	void UpdateChargeBeginCheck();
	void UpdateChargingState( float DeltaTime );
	void UpdatePropulsionLock( float DeltaTime );
	void UpdateFrictionRecoveryState( float DeltaTime );

	bool RequestChargeDash();

	float GetCurrentChargeRatio() const;
	bool CanExecuteChargedAction() const;
	bool IsPlayingChargeAttackMontage() const;

	bool ShouldContinueChargeCombo() const;

	void UpdateChargeComboIndex();
	void InterruptCurrentChargeAction();
	void PrepareOwnerForCharge();
	void StartChargeState();
	void PlayChargeStartMontage();
	void ApplyChargeStartMovement();
	void StopCurrentChargeMontages();
	void ClearChargeState();
	void ResetChargedActionState();
	void OnEndAction();

	void OnStartChargeDash();
	void OnStartChargeAttack();
	void OnStartChargeJump();

	void ExecuteChargePropulsion();	// チャージ攻撃やチャージダッシュの推進力の実行
	void OnStartLightAttack();
	FVector GetHomingDirection( const FVector& InDefaultDir, float InDebugDuration = 0.0f ) const;
	bool IsTargetableActor( const AActor* InActor ) const;

	struct FChargePropulsionSettings
	{
		float MinPower = 0.0f;
		float MaxPower = 0.0f;
		float Speed = 0.0f;
		float LockTime = 0.0f;
		float GhostTrailDuration = 0.0f;
	};
	FChargePropulsionSettings BuildChargePropulsionSettings( float ChargeRate ) const;

	void SpawnChargeEffect();
	void SpawnChargedDashEffect();
	void DestroyChargeEffect();
	void DestroyChargedDashEffect();

	void StartGhostTrailSpawing( float Duration );
	void UpdateGhostTrails( float DeltaTime );
	void SpawnGhostTrail();
	class UMaterialInterface* GetCurrentGhostTrailMaterial() const;

	void DrawChargeGaugeUI(
		float InCurrentTime,
		float InMaxTime,
		float InMinThreshold,
		const FVector& InPlayerLocation,
		class APlayerController* InPlayerController
	);

	// helper
	FName GetChargeStartAnimTag() const;
	FName GetChargeLoopAnimTag() const;

	FVector GetActorYawForwardDirection() const;
	FVector ProjectDirectionToGround( const FVector& InDirection ) const;

	const class UTidePlayerParamDataAsset* GetPlayerParams() const;
	class UCharacterMovementComponent* GetCharacterMovement() const;
	class UExCameraSubsystem* GetCameraSubsystem() const;

private:
	// チャージ中関連 ---

	EChargeActionType CurrentChargeActionType = EChargeActionType::None;
	bool bChargeInputHeld = false;		// ボタンが現在ホールド（長押し）されているか

	bool bIsCharging = false;
	bool bHasReachedMaxCharge = false;
	FAutomaticTimer CurrentChargeTimer;

	bool bWasDashingBeforeCharge = false;	// チャージ開始時にダッシュ中だったかどうかのフラグ

	UPROPERTY( Transient )
	TObjectPtr<class UNiagaraComponent> SpawnedChargeEffect;

	// チャージ後関連 ---

	FAutomaticTimer ChargedActionLockTimer;
	FAutomaticTimer FrictionRecoveryTimer;	// ダッシュ後の摩擦回復のためのタイマー

	FVector CachedChargeDashDirection = FVector::ZeroVector;	// 突進中の等速移動を維持するためのキャッシュ変数
	float CachedChargeDashSpeed = 0.0f;

	UPROPERTY( Transient )
	TObjectPtr<class UNiagaraComponent> SpawnedChargedDashEffect;

	// 残像関連 ---

	struct FGhostTrailData
	{
		TObjectPtr<class UPoseableMeshComponent> MeshComponent;
		TArray<TObjectPtr<class UMaterialInstanceDynamic>> MIDs;
		float ElapsedTime = 0.0f;
		float Lifespan = 1.0f;
	};
	TArray<FGhostTrailData> ActiveGhostTrails;

	FAutomaticTimer GhostTrailSpawnTimer;
	float RemainingGhostTrailDuration = 0.0f;

	// コンボ関連 ---

	int32 MaxChargeComboCount = 4;
	int32 CurrentChargeComboIndex = 1;

//	FCameraModeHandle ChargingCameraHandle;
	FCameraModeHandle ChargeDashCameraHandle;
	FAutomaticTimer ChargeDashCameraPopTimer;
};
