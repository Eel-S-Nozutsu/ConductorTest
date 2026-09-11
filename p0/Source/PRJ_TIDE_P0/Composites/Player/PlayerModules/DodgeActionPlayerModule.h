// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "DodgeActionPlayerModule.generated.h"

UENUM()
enum class EDodgeState : uint8
{
	Idle,
	InputWait,
	Stepping
};

UCLASS()
class UDodgeActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( class ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	bool IsDodging() const;
	void CancelDodge();
	void OnDodgeSuccess();

	// 通常の発動条件（クールダウン・状態タグ）を無視して強制的に回避を開始する。
	// やられ（吹き飛び）キャンセルなど、状態タグが揃っていない状況から出す用途
	void ForceDodge();

	bool HasDodgeInputBuffered() const;

#if !UE_BUILD_SHIPPING
	void DrawDebugImGui();
#endif

private:
	void OnStartStep( const FVector2D& InputVector );
	void OnEndStep();

	void ClearState();
	void StartCooldown();

	void OnEndDodgeSuccess();
	void ResetGlobalTimeDilation();

	void UpdateGravityLock( float DeltaTime );

	void StartGhostTrailSpawing( float Duration );
	void UpdateGhostTrails( float DeltaTime );
	void SpawnGhostTrail();

	// ポーズ複製（PoseableMesh＋フェード用MID）の共通生成。残像・回避成功ポーズで共用
	class UPoseableMeshComponent* BuildGhostPoseMesh( class UMaterialInterface* Material, TArray<TObjectPtr<class UMaterialInstanceDynamic>>& OutMIDs );

	// 回避成功時、左右へポーズを2体生成し横に広がって戻す演出（1波目を出しつつ2波目を予約）
	void SpawnDodgeSplitPoses();
	void SpawnDodgeSplitWave( class UMaterialInterface* Material, float MaxOffset, float Duration );
	void UpdateDodgeSplitPoses( float DeltaTime );

private:
	EDodgeState CurrentState = EDodgeState::Idle;
	FAutomaticTimer StepTimer;
	FAutomaticTimer InputWaitTimer;

	int32 CurrentDodgeCount = 0;
	bool bNextDodgeReserved = false;
	FAutomaticTimer CooldownTimer;

	FTimerHandle GlobalTimeDilationResetTimer;

	UPROPERTY( Transient )
	TObjectPtr<class UNiagaraComponent> SpawnedDodgeEffect;

	// フェードアウト管理用の構造体
	struct FGhostTrailData
	{
		TObjectPtr<class UPoseableMeshComponent> MeshComponent;
		TArray<TObjectPtr<class UMaterialInstanceDynamic>> MIDs;
		float ElapsedTime = 0.0f;
		float Lifespan = 1.0f;
	};
	// 現在画面に出ている残像のリスト
	TArray<FGhostTrailData> ActiveGhostTrails;

	// 連続スポーン制御用
	FAutomaticTimer GhostTrailSpawnTimer;
	float RemainingGhostTrailDuration = 0.0f;

	// 回避成功時の左右分身ポーズ（プレイヤーに追従しつつ横に広がって戻る）
	struct FDodgeSplitPose
	{
		TObjectPtr<class UPoseableMeshComponent> MeshComponent;
		TArray<TObjectPtr<class UMaterialInstanceDynamic>> MIDs;
		float LateralSign = 1.0f;	// 広がる向き（右+1 / 左-1）
		float ElapsedTime = 0.0f;
		float Duration = 0.25f;
		float MaxOffset = 120.0f;
	};
	TArray<FDodgeSplitPose> ActiveDodgeSplitPoses;

	// 2波目（遅れて出るもう1組）の予約
	bool bSecondWavePending = false;
	float SecondWaveDelayRemaining = 0.0f;

	FAutomaticTimer GravityLockTimer;

	FVector CachedDodgeDirection = FVector::ZeroVector;

	bool bIsSteerableDodge = false;	// 今回の回避が「旋回可能な前方回避」であるかを記憶するフラグ
};
