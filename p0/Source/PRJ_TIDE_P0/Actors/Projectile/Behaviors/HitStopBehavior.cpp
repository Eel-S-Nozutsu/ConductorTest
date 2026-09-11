// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "HitStopBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

void UHitStopBehavior::OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result)
{
	// 実際にダメージが通ったヒットのときだけ出す ※無敵などでは出さない
	if (!Projectile || Result != EDamageResult::Hit) return;

	// 被弾した相手と発射者にヒットストップ。相手はHitActorを使う
	// ※Hit.GetActor()はオーバーラップ経由で別の弾を指すことがあり、止めると不具合になる
	HitStopUtil::ApplyHitStop(HitActor, Duration, Dilation);
	HitStopUtil::ApplyHitStop(Projectile->GetOwner(), Duration, Dilation);
}
