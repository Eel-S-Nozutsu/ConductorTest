// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "HomingProjectileBehavior.generated.h"

/**
 * 旋回速度制限ホーミング
 * 
 * 進行方向を対象方向へ毎秒だけ曲げる (速さは維持)
 * 水平と上下を分離しそれぞれ別レートで制限
 * 残り寿命に比例して旋回性能を落としてみる
 *
 * 追従対象はAEnemyProjectile::HomingTargetを参照 (未指定ならOnLaunchでPCを自動取得)
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "ホーミング"))
class PRJ_TIDE_P0_API UHomingProjectileBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 水平(ヨー)旋回速度の下限 ※発射ごとに[Min, Max]の一様乱数で確定
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float TurnRateMinDegPerSec = 90.0f;

	// 水平(ヨー)旋回速度の上限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float TurnRateMaxDegPerSec = 150.0f;

	// 上下(ピッチ)旋回速度。ヨーと独立 ※高台から低いPCへ届かせたいときは大きくする
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PitchTurnRateDegPerSec = 200.0f;

	// 寿命終盤の旋回最小スケール ※発射直後が1、寿命終盤がこの値
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DecayMinScale = 0.3f;

	// 発射後この秒数だけ直進してから追従を開始 ※0で即追従
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float BeginDelay = 0.0f;

	// 追従を停止するまでの秒数 ※0で寿命まで追従し続ける
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ActiveDuration = 0.0f;

	// 対象とこの距離以内まで近づいたら追従を打ち切る ※0で無効
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float StopDistance = 0.0f;

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual bool WantsTick() const override { return true; }

private:

	TWeakObjectPtr<USceneComponent> Target; // 追従対象 ※OnLaunchでPC取得

	float Elapsed = 0.0f; // 発射からの経過秒
	float SteerElapsed = 0.0f; // 追従開始からの経過秒
	float ActiveTurnRate = 0.0f; // この弾で確定した旋回速度 (度/秒)

	bool bBegun = false; // 追従開始済みか
	bool bSteering = false; // 現在ステアリング中か

};
