// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ProjectileBehavior.generated.h"

class AEnemyProjectile;
class AActor;
struct FHitResult;
enum class EDamageResult : uint8;

/**
 * 弾の挙動モジュール基底
 * AEnemyProjectileがBehaviors配列として複数所有しライフサイクルの各点で呼び出す
 * Movement系は排他1個Effect系は複数を想定
 *
 * プール再利用に備えて実行時状態は必ずOnLaunchで初期化すること
 * (OnSpawnだけ呼ばれてOnLaunch前に消される経路 = 待機中の消滅 があるため保険は残す)
 *
 * OnSpawnが呼ばれた弾には必ず1回だけOnDeactivateが呼ばれる (返却/破棄/レベル遷移を問わず)
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, NotBlueprintable, CollapseCategories)
class PRJ_TIDE_P0_API UProjectileBehavior : public UObject
{
	GENERATED_BODY()

public:

	// 生成時 (出現・待機開始前) に1回呼ばれる。常駐VFXなど待機中から見せたいビジュアルの
	// 起動はここ ※プール再利用に備え実行時状態の初期化も
	virtual void OnSpawn(AEnemyProjectile* Projectile) {}
	// 発射時 (移動開始) に1回呼ばれる。移動・狙い等の起動はここ。待機ありなら待機後に呼ばれる
	virtual void OnLaunch(AEnemyProjectile* Projectile) {}
	// 毎フレーム呼ばれる ※WantsTick()がtrueのビヘイビアが1つ以上必要
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) {}
	// ダメージ処理後弾が消滅する直前に呼ばれる
	// HitActorは実際にダメージを与えた相手 (オーバーラップ経由でも確実に取れる
	// Hit.GetActor()はSweepResultが不正なとき誤った相手を返すため使わないこと)
	// Resultは実際にダメージが通ったか(Hit)などの判定結果
	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) {}
	// 地形ヒット時この弾を消滅させてよいか
	virtual bool ShouldConsumeOnHit(AEnemyProjectile* Projectile, const FHitResult& Hit, bool bDefault) { return bDefault; }
	// 消滅時の演出/終端イベント (保険炸裂・デスVFX・カメラ揺れ等)。静かな消滅
	// (スポナー範囲外フェード等) では呼ばれないので、必ず通したい後始末はOnDeactivateへ書くこと
	// 消滅理由で出し分けたいならProjectile->GetEndReason()を見る
	virtual void OnExpire(AEnemyProjectile* Projectile) {}
	// 消滅時の後始末。OnSpawnと対でプール返却/破棄/レベル遷移を問わず必ず1回呼ばれる
	// 外部へ副作用を残したビヘイビアはここで必ず解除すること
	// (VFXスポーン・ダメージ・カメラ揺れといった新規の副作用はOnExpire側の担当)
	virtual void OnDeactivate(AEnemyProjectile* Projectile) {}
	// OnTickを必要とするか
	virtual bool WantsTick() const { return false; }

};
