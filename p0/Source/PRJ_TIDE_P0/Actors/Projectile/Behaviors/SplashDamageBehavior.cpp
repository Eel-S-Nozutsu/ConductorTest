// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "SplashDamageBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

void USplashDamageBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// プール再利用で前回の炸裂フラグを残さない
	bDetonated = false;
}

void USplashDamageBehavior::OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result)
{
	if (!bDetonateOnHit) return;
	// 地形/敵に接触したらその位置で炸裂する。直撃相手は範囲ダメージから除外する
	Detonate(Projectile, Hit.ImpactPoint, HitActor);
}

void USplashDamageBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	// 接触も炸裂もしないまま寿命切れした場合の保険炸裂 (現在地)
	if (!bDetonateOnExpire || !Projectile) return;
	if (Projectile->WasHit() || Projectile->GetEndReason() != EProjectileEndReason::LifeSpanExpired)
	{
		return;
	}
	Detonate(Projectile, Projectile->GetActorLocation(), nullptr);
}

void USplashDamageBehavior::Detonate(AEnemyProjectile* Projectile, const FVector& Center, AActor* DirectHitActor)
{
	if (bDetonated || !Projectile) return;
	bDetonated = true;

	if (SplashRadius <= KINDA_SMALL_NUMBER || SplashDamage <= 0.0f)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// 炸裂した範囲ダメージエリアを可視化する。判定はこの場で完結する (以降は当たらない) ので、
	// 実際のダメージ発生期間に合わせて表示も1Fだけにする
	if (UTideGameSettings::Get()->bDebugDrawAttackHitbox)
	{
		DrawDebugSphere(Projectile->GetWorld(), Center, SplashRadius, 16, FColor::Red, false, -1.0f, 0, 1.5f);
	}
#endif

	AActor* OwnerActor = Projectile->GetOwner();

	TArray<AActor*> IgnoreActors = { OwnerActor };
	if (DirectHitActor)
	{
		IgnoreActors.Add(DirectHitActor);
	}

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(Projectile, Center, SplashRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn),
		  UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) },
		nullptr, IgnoreActors, OverlapActors);

	for (AActor* Actor : OverlapActors)
	{
		if (!TideCombatUtil::IsHostileTo(OwnerActor, Actor)) continue;

		IDamageable* Damageable = Cast<IDamageable>(Actor);
		if (!Damageable) continue;

		FDamageInfo Info;
		Info.BaseDamage = SplashDamage;
		Info.Instigator = OwnerActor;
		Info.HitReactionTag = Projectile->HitReactionTag;
		Info.HitResult.ImpactPoint = Actor->GetActorLocation();
		Info.HitResult.ImpactNormal = (Actor->GetActorLocation() - Center).GetSafeNormal();
		Damageable->ReceiveDamage(Info);
	}
}
