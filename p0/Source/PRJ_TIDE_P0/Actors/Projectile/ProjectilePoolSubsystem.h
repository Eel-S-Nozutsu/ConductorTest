// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectilePoolSubsystem.generated.h"

class AEnemyProjectile;
class UProjectileProfile;

/**
 * 弾のオブジェクトプール
 *
 * 取得は2段構え
 *  1. Acquire()で非アクティブな弾を取得
 *  2. 呼び出し側がプロパティを注入
 *  3. AEnemyProjectile::ActivateProjectile()で発射
 *
 * 返却は弾の消滅時にAEnemyProjectileから自動でRelease()される
 */
UCLASS()
class PRJ_TIDE_P0_API UProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	// 非アクティブな弾を取得しプロファイルを適用して返す 空きがなければ新規生成
	// 発射は呼び出し側がプロパティ注入後にActivateProjectile()で行う
	AEnemyProjectile* Acquire(UProjectileProfile* Profile,
		const FTransform& SpawnTransform, AActor* Owner, APawn* Instigator);

	// 弾をプールへ返却する (AEnemyProjectileの消滅処理から呼ばれる)
	void Release(AEnemyProjectile* Projectile);

private:

	// 生成済みの全弾
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnemyProjectile>> AllProjectiles;

	// プロファイルごとの空き弾リスト
	TMap<UProjectileProfile*, TArray<AEnemyProjectile*>> FreeLists;

};
