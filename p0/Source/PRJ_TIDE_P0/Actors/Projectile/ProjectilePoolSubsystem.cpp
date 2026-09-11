// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ProjectilePoolSubsystem.h"
#include "EnemyProjectile.h"
#include "ProjectileProfile.h"

#include "Engine/World.h"

AEnemyProjectile* UProjectilePoolSubsystem::Acquire(UProjectileProfile* Profile,
	const FTransform& SpawnTransform, AActor* Owner, APawn* Instigator)
{
	if (!Profile) return nullptr;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	AEnemyProjectile* Projectile = nullptr;

	// 同一プロファイルの空き弾を再利用 ※無効化済みは捨てる
	// プロファイル単位でバケツを分けるので再利用弾は複製済みビヘイビアをそのまま流用可能
	if (TArray<AEnemyProjectile*>* FreeList = FreeLists.Find(Profile))
	{
		while (FreeList->Num() > 0 && !Projectile)
		{
			AEnemyProjectile* Candidate = FreeList->Pop(EAllowShrinking::No);
			if (IsValid(Candidate)) Projectile = Candidate;
		}
	}

	// 空きがなければ新規生成する。bPoolManagedを立ててからBeginPlayさせて待機状態にす
	// る
	if (!Projectile)
	{
		const TSubclassOf<AEnemyProjectile> ActorClass = Profile->ProjectileActorClass
			? Profile->ProjectileActorClass
			: TSubclassOf<AEnemyProjectile>(AEnemyProjectile::StaticClass());

		Projectile = World->SpawnActorDeferred<AEnemyProjectile>(ActorClass,
			SpawnTransform, Owner, Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Projectile) return nullptr;

		Projectile->SetPoolOwner(this);
		Projectile->FinishSpawning(SpawnTransform); // BeginPlay ->自身をParkForPoolで待機させる
		AllProjectiles.Add(Projectile);
	}

	// 再利用準備 ※Owner/Instigator/座標を更新しプロファイルを適用
	Projectile->SetOwner(Owner);
	Projectile->SetInstigator(Instigator);
	Projectile->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Projectile->InitFromProfile(Profile);

	return Projectile;
}

void UProjectilePoolSubsystem::Release(AEnemyProjectile* Projectile)
{
	if (!IsValid(Projectile)) return;

	// 既に返却済み (非アクティブ) なら二重返却しない。空きリストへ同一弾が
	// 重複登録されるのを防ぐ (重複すると1つの弾を複数ショットが取り合う)
	if (!Projectile->IsActive()) return;

	Projectile->DeactivateForPool();

	// 適用中プロファイルのバケツへ返す (プール管理弾は必ずInitFromProfile済み)
	if (UProjectileProfile* Profile = Projectile->GetProfile())
	{
		FreeLists.FindOrAdd(Profile).Add(Projectile);
	}
}
