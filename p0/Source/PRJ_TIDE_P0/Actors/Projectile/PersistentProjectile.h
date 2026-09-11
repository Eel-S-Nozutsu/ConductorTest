// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyProjectile.h"
#include "PersistentProjectile.generated.h"

/**
 * Pawnとのオーバーラップ・地形へのブロッキングヒットいずれでも消滅せず、
 * 生存中ずっと当たり判定内の敵対アクターへ周期ダメージ（Damageを使用）を与え続ける弾
 *
 * MaxLifeTime経過でのみ消滅する（地形にぶつかった場合は物理的にその場へ止まり居座る）。
 */
UCLASS()
class PRJ_TIDE_P0_API APersistentProjectile : public AEnemyProjectile
{
	GENERATED_BODY()

public:
	// ドットダメージを与える間隔 (秒)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide", meta = (ClampMin = "0.01"))
	float DotTickInterval = 0.5f;

	// ドットダメージでヒットリアクションを取らせるか。falseなら継続ダメージ扱いにし
	// ヒットリアクション・カメラ揺れを毎ティック抑制する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	bool bDotUsesHitReaction = false;

	virtual void ActivateProjectile() override;

protected:

	virtual void Tick(float DeltaSeconds) override;

	virtual bool ShouldConsumeOnOverlap(AActor* OtherActor, const FHitResult& SweepResult) const override { return false; }
	virtual bool ShouldConsumeOnTerrainHit(AActor* OtherActor, const FHitResult& Hit) const override { return false; }
	virtual void ApplyExtraProfileParams(UProjectileProfile* Profile) override;
	// ドットダメージのTickは常に必要（Behaviorのwants Tickやデバッグ表示の有無に関わらず駆動する）
	virtual bool WantsProjectileTick() const override { return true; }

private:
	// 現在重なっているアクターごとの、前回ダメージからの経過時間 (秒)。
	// 毎フレームのオーバーラップ検出とセットで管理し、一瞬しか重ならない相手のすり抜けを防ぐ
	// （新規に重なった相手には即座に1回ダメージを与え、以降はDotTickInterval間隔で継続する）
	TMap<TWeakObjectPtr<AActor>, float> DotElapsedByActor;
};
