// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "SlidingHomingBehavior.generated.h"

/**
 * 滑走ホーミング
 * 
 * ProjectileMovement内蔵ホーミングでPCを追い浅い斜面では消えずに滑走し
 * 急斜面/壁で消滅 すれ違いでホーミングを打ち切る
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "滑走ホーミング"))
class PRJ_TIDE_P0_API USlidingHomingBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// ホーミング追従の強さ ※0でホーミングしない
	UPROPERTY(EditAnywhere)
	float HomingAccelerationMagnitude = 20000.0f;

	// 1秒あたりの最大旋回角 ※0で無制限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MaxTurnRateDeg = 100.0f;

	// 発射後ホーミングが始まるまでの遅延秒数
	UPROPERTY(EditAnywhere)
	float HomingStartDelay = 0.0f;

	// ホーミング持続秒数 ※0で無制限
	UPROPERTY(EditAnywhere)
	float HomingDuration = 1.0f;

	// 垂直方向もホーミングするか
	UPROPERTY(EditAnywhere)
	bool bHomingVertical = false;

	// PCとすれ違ったらホーミングを諦めるかどうか
	UPROPERTY(EditAnywhere)
	bool bDisableHomingOnPassby = true;

	// 地形に沿わせる最大傾斜角 ※これ以内は滑走し超えると壁として消滅
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSlideAngle = 45.0f;

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual bool ShouldConsumeOnHit(AEnemyProjectile* Projectile, const FHitResult& Hit, bool bDefault) override;
	virtual bool WantsTick() const override { return true; }

private:

	void DisableHoming();

	float Elapsed = 0.0f;
	bool bHomingStarted = false;
	bool bHomingActive = false;

};
