// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SlidePassiveGustBurst.generated.h"

class USphereComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * スライドパッシブ「突風」の範囲攻撃バースト。
 *
 * 突風バフが乗った次のチャージアクション発動時に足元へスポーンし、
 * スポーン直後に範囲内の敵対 IDamageable へ 1 回だけ範囲ダメージを与える（再アームなし）。
 * 見た目（Niagara）も内包し、短寿命で自己破棄する。Owner はスポーンしたキャラ（敵味方判定に使う）。
 *
 * 竜巻（ASlidePassiveTornado）と異なり、巻き上げ（IWindAffectable）や継続ダメージは持たない一発バースト。
 */
UCLASS()
class PRJ_TIDE_P0_API ASlidePassiveGustBurst : public AActor
{
	GENERATED_BODY()

public:
	ASlidePassiveGustBurst();

	// スポーン直後に呼ぶ。見た目・範囲・ダメージ・寿命を設定し、範囲ダメージを即時適用する
	void Activate( UNiagaraSystem* InVFX, float InRadius, float InDamage, float InLifeTime, float InScale );

	// 範囲ダメージ判定を止める（破棄前のフェードアウト中など、見た目だけ残したいとき）
	void SuspendDamage() { bDamageSuspended = true; }

protected:
	virtual void Tick( float DeltaTime ) override;

	// 範囲内の敵対 IDamageable へダメージを与える（同一対象は再アーム間隔ごと。竜巻と同じ毎フレーム判定）
	void ApplyBurstDamage();

public:
	// 範囲判定の球（Activate の Radius で拡縮される）
	UPROPERTY( VisibleAnywhere, Category = "Tide|GustBurst" )
	TObjectPtr<USphereComponent> DamageVolume;

	// バーストの見た目
	UPROPERTY( VisibleAnywhere, Category = "Tide|GustBurst" )
	TObjectPtr<UNiagaraComponent> BurstVFX;

	// ヒットリアクション種別
	UPROPERTY( EditAnywhere, Category = "Tide|GustBurst" )
	FGameplayTag HitReactionTag;

	// 同一対象への再ダメージ間隔（秒）。竜巻のスリップと同じ再アーム方式で多段ヒットを防ぐ
	UPROPERTY( EditAnywhere, Category = "Tide|GustBurst" )
	float DamageReArmInterval = 0.5f;

	// 判定範囲をデバッグ描画する
	UPROPERTY( EditAnywhere, Category = "Tide|GustBurst|Debug" )
	bool bDebugDrawVolume = false;

private:
	float LifeTime = 1.0f;
	float ElapsedTime = 0.0f;
	float Damage = 0.0f;
	bool bDamageSuspended = false;	// true の間は範囲ダメージを与えない（フェードアウト中など）

	// 同一対象へのダメージ再アーム管理（最後にダメージを与えた時刻）
	TMap<TWeakObjectPtr<AActor>, double> LastDamageTimes;

	// 初回接触時に抽選してキャッシュする 敵ごとの死亡吹き飛び方向 (+1=右 -1=左)
	TMap<TWeakObjectPtr<AActor>, float> LaunchSideByActor;
};
