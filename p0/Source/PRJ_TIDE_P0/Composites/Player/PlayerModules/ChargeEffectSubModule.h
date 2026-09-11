// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "ChargeEffectSubModule.generated.h"

class ATidePlayerCharacter;
class UChargeActionPlayerModule_V2;
class UNiagaraComponent;

/**
 * ChargeActionPlayerModule_V2 から演出処理（Niagara／残像／スキッド／フレネル／カメラ）を切り出したもの。
 * ゲームプレイ状態は OwnerModule / OwnerCharacter の public API 経由で読み取り、
 * 本クラスはエフェクト生成・描画のみを担当する（ゲームロジックは持たない）。
 */
UCLASS()
class UChargeEffectSubModule : public UObject
{
	GENERATED_BODY()

public:
	void Initialize( UChargeActionPlayerModule_V2* InOwnerModule, ATidePlayerCharacter* InOwnerCharacter );

	// --- スキッド ---
	void UpdateSkidEffects( float DeltaTime );
	void UpdateHitbackSkidEffects( float DeltaTime, bool bIsInKnockback );	// 両足骨から出す

	// --- 残像 ---
	void UpdateGhostTrails( float DeltaTime );
	void NotifyChargeStarted( float GhostTrailSpawnInterval );	// ブロック解除＋初回タイマーセット
	void ResetGhostTrailSpawnTimer();							// 強制リセット時にクリア
	bool HasActiveGhostTrails() const;
	bool IsGhostTrailCycleActive() const;						// HasActiveGhostTrails || ShouldSpawnGhostTrail

	void UpdateFresnelEffect( float DeltaTime );

	// --- Niagara チャージエフェクト ---
	void ActivateChargeEffect();
	void DeactivateChargeEffect();
	void SpawnChargeCompleteEffect();
	void SpawnChargedDashEffect();
	void DestroyChargedDashEffect();
	// 風切り（ループ）。前方オフセット＋Yaw＋180 で出し、int "Gear"(1~3) を渡す
	void SpawnChargedDashWindEffect();
	void DestroyChargedDashWindEffect();
	void SpawnActiveChargeEffect();
	void DestroyActiveChargeEffect();
	void SpawnChargeReleaseEffect();
	void UpdateSpawnedChargeEffectRotation();
	void NotifyGearShifted();	// ギアシフト時にエフェクトの色・速度を更新

	// --- ドリフトブースト演出（足元の加速 VFX）---
	void UpdateChargeBoostEffect( float DeltaTime );
	void StopChargeBoostEffect();		// ループを止め、再点火用フラグをリセットする
	void NotifyDriftSparkGearUpBurst();	// 火花サイズを一定時間だけ満額へ跳ね上げる

	// ドリフトしていなくても火花を強制表示する（突風成立時など）。サイズ・尺はギアアップバーストと同じ
	void NotifyDriftSparkForcedBurst();

	// --- ギア表示 UI（漢字ギアのポップ表示）---
	void NotifyChargeGearUIPop();	// チャージ開始・ギアシフト時に開始
#if !UE_BUILD_SHIPPING
	// ImGui 製の暫定 UI なので Shipping には出さない（正式 UI 実装時はここごと差し替え）
	void DrawChargeGaugeUI();
#endif

private:
	void SpawnSkidEffect();

	void ActivateChargeBoostEffect();
	void DeactivateChargeBoostEffect();
	float GetDriftSparkScale() const;	// VelocityRate に掛ける倍率。バーストの進行を反映する

	void SpawnGhostTrail();
	bool ShouldSpawnGhostTrail() const;
	class UMaterialInterface* GetCurrentGhostTrailMaterial() const;

private:
	UPROPERTY( Transient )
	TObjectPtr<UChargeActionPlayerModule_V2> OwnerModule;

	UPROPERTY( Transient )
	TObjectPtr<ATidePlayerCharacter> OwnerCharacter;

	FAutomaticTimer SkidEffectSpawnTimer;
	FAutomaticTimer HitbackSkidSpawnTimer;

	// --- 残像 ---
	struct FGhostTrailData
	{
		TObjectPtr<class UPoseableMeshComponent> MeshComponent;
		TArray<TObjectPtr<class UMaterialInstanceDynamic>> MIDs;
		float ElapsedTime = 0.0f;
		float Lifespan = 1.0f;
	};
	TArray<FGhostTrailData> ActiveGhostTrails;
	FAutomaticTimer GhostTrailSpawnTimer;
	bool bBlockGhostTrailSpawn = false;	// 強制リセット後、次のチャージ開始まで新規スポーンを禁止する

	// --- フレネル ---
	float TargetFresnelIntensity = 0.0f;
	float CurrentFresnelIntensity = 0.0f;
	float FresnelBlinkPhase = 0.0f;

	// --- Niagara チャージエフェクト ---
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> SpawnedChargeEffect;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> SpawnedChargeCompleteEffect;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> ActiveChargeEffect;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> SpawnedChargedDashEffect;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> SpawnedChargedDashWindEffect;

	// --- ドリフトブースト演出 ---
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> ActiveChargeBoostEffect;

	bool bWasChargeBoosting = false;	// 開始エフェクトの立ち上がり検出用

	// 火花サイズバーストの残り時間 = Hold + BlendOut。BlendOut 帯に入ってから通常サイズへ戻す
	FAutomaticTimer DriftSparkBurstTimer;

	// 強制バースト中はドリフト判定を無視して火花を出し、サイズは満額固定にする
	bool bDriftSparkBurstForced = false;

	// --- ギア表示 UI ---
	// 拡縮＋フェードの総持続時間。ギアシフト間隔に合わせ、次のギアがポップする瞬間に消え切らせる
	float GetGearUIPopDuration() const;

	FAutomaticTimer GearUIPopTimer;
	static constexpr float UI_PopInDuration = 0.3f;		// 出現（ポップ）にかかる秒数
	static constexpr float UI_PopHoldDuration = 0.35f;	// 完全に不透明なまま留まる秒数
	static constexpr float UI_PopMinOutDuration = 0.1f;	// フェードアウトに最低限確保する秒数
	static constexpr float UI_PopDuration = 1.15f;		// 総持続時間のフォールバック（DA 未設定時）
};
