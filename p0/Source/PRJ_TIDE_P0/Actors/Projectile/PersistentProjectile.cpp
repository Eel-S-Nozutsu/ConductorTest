// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#include "PersistentProjectile.h"
#include "ProjectileProfile.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "GameFramework/Pawn.h"

void APersistentProjectile::ApplyExtraProfileParams(UProjectileProfile* Profile)
{
	if (!Profile) return;

	DotTickInterval = Profile->DotTickInterval;
	bDotUsesHitReaction = Profile->bDotUsesHitReaction;
}

void APersistentProjectile::ActivateProjectile()
{
	// プール再利用に備えアクターごとの経過時間マップを初期化
	DotElapsedByActor.Reset();

	Super::ActivateProjectile();
}

void APersistentProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!CollisionComp) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	// オーバーラップ検出自体は間引かず毎フレーム行う（間引くと一瞬しか重ならない相手を取りこぼすため）
	TArray<AActor*> Overlapping;
	CollisionComp->GetOverlappingActors(Overlapping, APawn::StaticClass());

	TSet<TWeakObjectPtr<AActor>> CurrentActors;

	// 敵対判定・IDamageableキャストは TryDamageActor が内部で行う（非対象なら true を返すだけで無害）
	for (AActor* Other : Overlapping)
	{
		if (!Other || Other == OwnerActor) continue;

		// TWeakObjectPtrのハッシュは生ポインタのハッシュと一致しないため、
		// Find/Addは必ず明示的にTWeakObjectPtrへ変換してから呼ぶ（そのまま渡すと常に不一致になる）
		const TWeakObjectPtr<AActor> WeakOther(Other);
		CurrentActors.Add(WeakOther);

		float* Elapsed = DotElapsedByActor.Find(WeakOther);
		if (!Elapsed)
		{
			// 新規に重なった相手: 即座に1回ダメージを与える（すり抜け対策）
			EDamageResult Result = EDamageResult::Immune;
			TryDamageActor(Other, FHitResult(), Result, /*bIsDamageOverTime=*/!bDotUsesHitReaction);
			DotElapsedByActor.Add(WeakOther, 0.0f);
			continue;
		}

		*Elapsed += DeltaSeconds;
		if (*Elapsed >= DotTickInterval)
		{
			*Elapsed -= DotTickInterval;
			EDamageResult Result = EDamageResult::Immune;
			TryDamageActor(Other, FHitResult(), Result, /*bIsDamageOverTime=*/!bDotUsesHitReaction);
		}
	}

	// 離脱した相手のエントリは削除する（再接触時にまた即座にダメージが入るように）
	for (auto It = DotElapsedByActor.CreateIterator(); It; ++It)
	{
		if (!CurrentActors.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
}
