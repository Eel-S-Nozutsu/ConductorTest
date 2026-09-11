// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "SplashDamageBehavior.generated.h"

/**
 * 着弾時の範囲ダメージ (Effect)
 *
 * 炸裂のトリガーは弾ライフサイクルのフックに委ねる (飛び方 = Movementビヘイビアからは独立):
 *  - 接触 (OnHit): 地形/敵に当たった位置で炸裂。直撃相手は範囲ダメージから除外する
 *  - 寿命切れ (OnExpire): 接触しないまま寿命切れした場合の保険炸裂 (現在地)
 *
 * どのMovementとも合成できる (直進/放物線/弧/ホーミング)。多重炸裂は1発ガードで防ぐ。
 * SplashDamageは技側が1発ごとにInjectAttackDamageで上書きする運用。
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "範囲ダメージ (着弾炸裂)"))
class PRJ_TIDE_P0_API USplashDamageBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 炸裂半径
	UPROPERTY(EditAnywhere, Category = "Splash")
	float SplashRadius = 200.0f;

	// 炸裂ダメージ量
	UPROPERTY(EditAnywhere, Category = "Splash")
	float SplashDamage = 20.0f;

	// 接触時に炸裂する
	UPROPERTY(EditAnywhere, Category = "Splash")
	bool bDetonateOnHit = true;

	// 接触しないまま寿命切れした場合に炸裂する ※保険
	UPROPERTY(EditAnywhere, Category = "Splash")
	bool bDetonateOnExpire = true;

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;

private:

	// 範囲ダメージを1度だけ発生させる (多重炸裂防止)
	void Detonate(AEnemyProjectile* Projectile, const FVector& Center, AActor* DirectHitActor);

	bool bDetonated = false;

};
