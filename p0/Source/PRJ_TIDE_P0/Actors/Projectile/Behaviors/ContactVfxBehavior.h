// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "ContactVfxBehavior.generated.h"

class UNiagaraSystem;

/**
 * 接触/消滅効果 ヒット時と寿命切れ時に単発のNiagaraを発生させる
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "接触/消滅VFX"))
class PRJ_TIDE_P0_API UContactVfxBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// PC接触時のNiagara
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> HitVfx;

	// 背景(地形/壁)接触時のNiagara ※未設定なら何も出さない
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> BackgroundVfx;

	// 寿命切れ時のNiagara
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> DeathVfx;

	// Niagaraのスケール
	UPROPERTY(EditAnywhere)
	FVector Scale = FVector(1.0f);

	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;

};
