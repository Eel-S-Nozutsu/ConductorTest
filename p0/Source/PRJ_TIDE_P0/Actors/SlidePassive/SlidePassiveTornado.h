// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SlidePassiveTornado.generated.h"

class UCapsuleComponent;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * スライドパッシブで発生する竜巻アクター。
 *
 * 円の中心に発生し、寿命の間、範囲内の敵対 IDamageable へ一定間隔でスリップダメージを与える。
 * 見た目（Niagara）も内包し、スポーン元（SlidePassivePlayerModule）から Activate() で
 * 実アセット・スケール・寿命を受け取る。Owner はスポーンしたキャラ（敵味方判定に使う）。
 *
 * スリップダメージ（IDamageable）と巻き上げ（IWindAffectable）を範囲内の対象に与える。
 */
UCLASS()
class PRJ_TIDE_P0_API ASlidePassiveTornado : public AActor
{
	GENERATED_BODY()

public:
	ASlidePassiveTornado();

	// スポーン直後に呼ぶ。見た目・スケール・寿命を設定して判定を有効化する。bInIsLarge（竜巻・大か）は浮かない敵の
	// リアクション選択に使い、InJumpBoostMultiplier は竜巻ジャンプの強化倍率（小／大の固定値）を DA から渡す
	void Activate( UNiagaraSystem* InVFX, float InScale, float InLifeTime, bool bInIsLarge, float InJumpBoostMultiplier );

protected:
	virtual void Tick( float DeltaTime ) override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;

	// 範囲内の敵対 IDamageable へ、再アーム間隔ごとにスリップダメージを与える
	void ApplySlipDamage();

	// 範囲内の IWindAffectable へ巻き上げの影響を配る（範囲の出入りで Enter/Exit を発火）
	void ApplyWind( float DeltaTime );
	// 影響中の全対象へ Exit を通知して解放する（消滅・破棄時）
	void ReleaseAllWindTargets();

public:
	// ダメージ判定の範囲（カプセル）。Activate のスケールで拡縮される
	UPROPERTY( VisibleAnywhere, Category = "Tide|Tornado" )
	TObjectPtr<UCapsuleComponent> DamageVolume;

	// 竜巻の見た目
	UPROPERTY( VisibleAnywhere, Category = "Tide|Tornado" )
	TObjectPtr<UNiagaraComponent> TornadoVFX;

	// スリップダメージ1回あたりの量（スポーン時に AttackParameterTable の
	// AttackType.Player.SlidePassiveTornadoSmall/Large 行から上書きされる。行が無い場合はこの既定値がフォールバック）
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Damage" )
	float Damage = 3.0f;

	// 同一対象への再ダメージ間隔（秒）＝スリップの刻み間隔
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Damage" )
	float DamageReArmInterval = 0.5f;

	// スリップダメージ時のヒットリアクション種別
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Damage" )
	FGameplayTag HitReactionTag;

	// カプセル判定の基準半径（cm）。Activate のスケールが乗る
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Collision" )
	float BaseRadius = 300.0f;

	// カプセル判定の基準ハーフハイト（cm）。Activate のスケールが乗る
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Collision" )
	float BaseHalfHeight = 300.0f;

	// 巻き上げの上昇速度（cm/秒）
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Wind" )
	float LiftSpeed = 800.0f;

	// 中心へ寄せる水平引き込み速度（cm/秒）
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Wind" )
	float PullSpeed = 600.0f;

	// 中心の高さからの最大持ち上げ高さ（cm）。Activate のスケールが乗る。0 以下で無制限
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Wind" )
	float BaseMaxLiftHeight = 500.0f;

	// ON にするとカプセル判定範囲を毎フレーム描画する
	UPROPERTY( EditAnywhere, Category = "Tide|Tornado|Debug" )
	bool bDebugDrawVolume = false;

private:
	float LifeTime = 0.0f;		// 0 以下なら寿命なし（手動破棄まで残る）
	float ElapsedTime = 0.0f;
	float CurrentScale = 1.0f;	// Activate で受け取ったスケール（判定・高さに乗る）
	bool  bIsLarge = false;		// 竜巻 (大) かどうか。スリップダメージの WindTier に乗せる
	float JumpBoostMultiplier = 1.0f;	// PL 竜巻ジャンプ強化倍率（小／大の固定値。見た目スケールとは独立）

	// 同一対象へのスリップ再アーム管理（最後にダメージを与えた時刻）
	TMap<TWeakObjectPtr<AActor>, double> LastDamageTimes;

	// 現在巻き上げ影響中の対象（範囲の出入りで Enter/Exit を発火するために保持）
	TSet<TWeakObjectPtr<AActor>> WindAffectedActors;
};
