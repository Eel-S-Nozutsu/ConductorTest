// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "BallisticBehavior.generated.h"

/**
 * 物理放物線弾道 (着弾点へ重力で落とす) — 飛び方のみ (Movement)
 *
 * 着弾点と飛行時間から初速を逆算しProjectileMovementに重力込みで飛ばさせる。
 * 速度は物理的に連続で継ぎ目がなく、ベジェ弧のようなlinear-t由来の減速は起きない。
 * 「とにかく着弾点へ当てたい」用途 (散弾爆撃 / 着弾リング系) はこれを使う。
 *
 * 着弾点は地面へ射影しておけば、飛行時間ぶんで物理的にそこへ落ちて地形に接触する
 * (弾はWorldStaticをBlockするためOnHitが発火し消滅する)。
 * 範囲ダメージが要る場合はEffectのUSplashDamageBehaviorを合成する (この弾自体は与ダメしない)。
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "物理放物線 (着弾点へ)"))
class PRJ_TIDE_P0_API UBallisticBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 着弾までの飛行秒数
	UPROPERTY(EditAnywhere, Category = "Ballistic", meta = (ClampMin = "0.05"))
	float FlightDuration = 1.5f;

	// 重力スケール ※1.0でワールド重力
	UPROPERTY(EditAnywhere, Category = "Ballistic", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	// 着弾点を技側が1発ごとに注入 ※発射前にFindBehavior経由で呼ぶ
	void SetTargetLocation(const FVector& WorldLocation);

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;

private:

	// ---- 1発ごとの設定 (OnLaunchで取り込み後リセット) ----
	FVector CfgTarget = FVector::ZeroVector;
	bool bCfgTarget = false;

	// ---- 実行時状態 (OnLaunchで初期化) ----
	FVector Target = FVector::ZeroVector;

};
