// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"

#include <bitset>

#include "SpecialProjectileBehavior.generated.h"


UCLASS(DontCollapseCategories, meta = (DisplayName = "特殊弾"))
class PRJ_TIDE_P0_API USpecialProjectileBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:
	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual bool WantsTick() const override { return true; }

private:
	UPROPERTY(EditAnywhere)
	float BeginHormingTime = 0.0f; // 追従開始までの遅延時間 (秒)
	UPROPERTY(EditAnywhere)
	float HomingRateAccel = 1.0f; // 追従率の加速度 (1秒で1.0に達する)
	UPROPERTY(EditAnywhere)
	float EndHomingTime = 0.0f; // 追従終了までの時間 (秒)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float MaxTurnAngleDegPerSec = 60.0f; // 1秒あたりの最大旋回角

	TWeakObjectPtr<USceneComponent> Target; // 追従対象 (OnLaunchでPC取得)
	float Elapsed = 0.0f; // 経過時間 (秒)
	float HomingRate = 0.0f; // 追従率 (0.0～1.0)
};
