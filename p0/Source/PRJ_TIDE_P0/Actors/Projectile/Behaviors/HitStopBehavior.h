// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "HitStopBehavior.generated.h"

/**
 * 着弾時のヒットストップ
 * 実際にダメージが通ったヒットのときだけ対象と発射者の両方を一時停止する
 * 無敵や回避では発火しない
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "ヒットストップ"))
class PRJ_TIDE_P0_API UHitStopBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// ヒットストップ持続秒数
	UPROPERTY(EditAnywhere)
	float Duration = 0.1f;

	// ヒットストップ時のTimeDilation ※0で停止
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Dilation = 0.1f;

	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) override;

};
