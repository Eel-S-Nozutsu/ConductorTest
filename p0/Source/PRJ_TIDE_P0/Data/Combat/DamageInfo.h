// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PRJ_TIDE_P0/Data/Combat/DeathRagdollTypes.h"
#include "DamageInfo.generated.h"

UENUM(BlueprintType)
enum class EDamageResult : uint8
{
	Hit,      // ダメージ確定
	Evaded,   // 回避成功
	Blocked,  // ガード(将来用)
	Immune,   // 無敵など受け付け不可
};

// 竜巻スリップダメージの段階。浮かない敵が大小で別リアクションを選ぶために使う
UENUM(BlueprintType)
enum class EWindDamageTier : uint8
{
	None,     // 竜巻由来ではない通常ダメージ
	Small,    // 竜巻 (小)
	Large,    // 竜巻 (大)
};

/**
 * ダメージ情報
 */
USTRUCT(BlueprintType)
struct FDamageInfo
{
	GENERATED_BODY()

	// 加害者
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;
	// 生のヒット情報
	UPROPERTY()
	FHitResult HitResult;

	// 基本ダメージ
	UPROPERTY()
	float BaseDamage = 0.0f;
	// 耐久削り値
	UPROPERTY()
	float StaggerDamage = 0.0f;

	// 攻撃種類
	UPROPERTY()
	FGameplayTag AttackTypeTag;
	// チャージ攻撃のギア種別 (Charge.Gear1〜4)。非チャージ攻撃は未設定
	// PC側がチャージ発動時にセットする。敵のガード判定に使用する
	UPROPERTY()
	FGameplayTag ChargeGearTag;

	// ノックバック方向の上書き。ゼロベクトルの場合はInstigator位置から自動計算する
	UPROPERTY()
	FVector KnockbackDirectionOverride = FVector::ZeroVector;

	// 死亡ラグドールの吹き飛び方向だけを上書きする (水平方向
	// 大きさはDeathLaunchForceを使う)
	// 通常死亡は「加害者から遠ざかる」向きだが、突風パッシブのように横へ飛ばしたい攻撃が立てる
	// KnockbackDirectionOverrideと違い生存時ノックバックには影響せず、
	// 死亡経路だけが参照する
	UPROPERTY()
	FVector DeathLaunchDirectionOverride = FVector::ZeroVector;

	// 死亡ラグドールの回転モードを敵ごとの既定 (DeathSpin.Mode) より優先して上書きする
	// 突風パッシブのように「この攻撃で死んだら必ずきりもみ」にしたい攻撃が立てる
	// 回転速度・インパルス等は敵のDeathSpinをそのまま使い、モードだけ差し替える
	UPROPERTY()
	bool bOverrideDeathSpinMode = false;
	UPROPERTY()
	EDeathRagdollSpinMode DeathSpinModeOverride = EDeathRagdollSpinMode::Corkscrew;

	// 死亡ラグドールの回転速度 (度/秒) を敵既定 (DeathSpin.SpeedDegrees)
	// より優先して上書きする。負値 = 上書きしない。回転を強めたい攻撃 (突風パッシブ等) が立てる
	UPROPERTY()
	float DeathSpinSpeedOverride = -1.0f;

	// 死亡ラグドールの吹き飛び力を敵既定 (DeathLaunchForce) より優先して上書きする
	// 負値 = 上書きしない。攻撃ごとに飛距離を調整したいとき (突風パッシブ等) が立てる
	UPROPERTY()
	float DeathLaunchForceOverride = -1.0f;

	// 死亡ラグドールの上方向の力を上書きする。負値 = 上書きしない (水平力の0.5倍を使う)
	// 水平は抑えつつ高く打ち上げたい等、軌道の高さを水平と独立に調整したい攻撃が立てる
	UPROPERTY()
	float DeathLaunchUpForceOverride = -1.0f;
	// ヒットリアクション種別 (HitReaction.Light /
	// HitReaction.Knockback等)
	UPROPERTY()
	FGameplayTag HitReactionTag;

	// 継続ダメージ (DoT。しびれ床など) 由来か
	// trueのときプレイヤーは被弾リアクション・カメラ揺れをスキップしHPだけ削る
	// (毎刻みののけぞり・揺れ連発を避ける)
	UPROPERTY()
	bool bIsDamageOverTime = false;

	UPROPERTY()
	bool bUseHitStop = false;
	UPROPERTY()
	float HitStopDuration = 0.1f;
	UPROPERTY()
	float HitStopDilation = 0.1f;

	UPROPERTY()
	FGameplayTag HitPartTag;

	// 竜巻スリップ由来か (None=通常)。浮かない敵が大小で別リアクションを選ぶために使う
	UPROPERTY()
	EWindDamageTier WindTier = EWindDamageTier::None;

	// 光輪を無条件で一撃破壊するか。スライドパッシブ (突風・竜巻) が立てる
	// 神技と同じ扱いになり、ギア・背後条件・ヒビ状態・通常SAを無視して破壊する
	// (完全無敵SuperArmor_Invincibleだけは尊重する)
	// 判定はTideCombatUtil::IsHaloBreakerDamage
	UPROPERTY()
	bool bBreaksHaloUnconditionally = false;

	// 壊れ物(ABreakableProp) を壊せる攻撃か。既定の壊れ物はプレイヤーの攻撃だけを受け付けるため、
	// 環境ギミック由来の攻撃(アリジゴクのコア爆発など) がこれを立てて明示的に許可する
	UPROPERTY()
	bool bCanBreakProps = false;

};
