// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "BossDataAsset.generated.h"

/**
 * 部位光輪の設定。HaloMeshがnullのときは光輪なしとして扱う
 */
USTRUCT(BlueprintType)
struct FPartHaloConfig
{
	GENERATED_BODY()

	// リング状スタティックメッシュ (null = 光輪なし)
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	TObjectPtr<class UStaticMesh> HaloMesh = nullptr;

	// アタッチ先ソケット名
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FName HaloSocket;

	// 腕・足でサイズを変えるスケール
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float HaloScale = 1.0f;

	// 破壊時にスポーンするジオメトリコレクション
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	TObjectPtr<class UGeometryCollection> HaloBreakGC = nullptr;

	// クラスターを砕くStrain強度
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakStrainMagnitude = 1000000.0f;

	// Strain/Scatterフィールドの作用半径
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakFieldRadius = 200.0f;

	// ラジアル散布力 (0 = 重力のみで落下)
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakScatterForce = 0.0f;

	// 破片フェードアウト秒数
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakFadeDuration = 0.5f;

	// 破片が存在する合計秒数 ※BreakFadeDuration以上にする
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakLifeSpan = 2.0f;

	// ディザフェード用マテリアルパラメータ名 (0=表示1=消滅)
	// ザコ光輪のUEnemyDataAsset::HaloDitherParamNameと同じ規約で、
	// M_Halo系が持つDitherAlphaを指す
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FName DitherParamName = TEXT("DitherAlpha");

	// 復活時のディザ表示秒数。DitherAlphaを1から0へフェードインする
	// 0 = ディザなしで即表示。フェード完了後に部位反応が解禁される
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float ReviveDissolveDuration = 0.5f;

	// 攻撃中の一時非表示/再表示フェード時間 (秒)
	// DitherAlphaを0(表示)<->1(消滅) で遷移させる。0 = フェードなしで即時切り替え
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float HaloHideDissolveDuration = 1.0f;

	// 攻撃発光マテリアルパラメータ名 (0=通常1=発光)。HaloComponentと同じ仕組み
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FName AttackGlowParamName = TEXT("AttackGlow");

	// ヒビのマテリアルパラメータ名 (0=無傷1=ヒビあり)。HaloComponentと同じ規約
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FName CrackParamName = TEXT("Crack");

	// ヒットしたが壊れなかったときの3軸回転ブレ
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FHaloHitShakeParams HitShake;

	// 展開中(光輪ヒット)のダメージ倍率。1.0=従来どおり、下げると光輪で硬くなる
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float HaloHitDamageMultiplier = 1.0f;

	// 無防備(本体ヒット)のダメージ倍率。大きいほど痛い
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BodyHitDamageMultiplier = 1.5f;

	// 破壊後、再生するまでのクールタイム
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	float BreakRegenCooldown = 8.0f;

};

/**
 * 部位エントリー
 * メッシュコンポーネントのタグで部位を識別する。
 * 壊れ方はザコの光輪と共通で、軽ギア(無/壱/弍)はヒビ→次の一発で破壊、重ギア(参/極)と神技は一撃で破壊する
 * 破壊時のリアクション(モンタージュ・VFX)も合わせて設定する
 */
USTRUCT(BlueprintType)
struct FPartEntry
{
	GENERATED_BODY()

	// 対象メッシュコンポーネントのタグ
	// AnimNotifyState_CommonAttackの"DamageLayer"タグと併用
	UPROPERTY(EditAnywhere)
	FName PartTag;

	// 同じ足の上下など、コリジョンが重なってヒットを奪い合う部位を束ねる名前 (未設定 = 束ねない)
	// 同グループに生きた部位が残っている間だけ、破壊済み部位がヒット判定から外れる
	UPROPERTY(EditAnywhere)
	FName HitAbsorbGroup;

	// trueのとき、この部位配下のLockOnTargetは通常ロックオン不可とし、
	// 複数部位破壊リアクション
	// (ダウン)中もしくはプレイヤーの神技ロックオン中だけロックオン可能にする
	// (ボスがbIsTargetableを駆動)
	UPROPERTY(EditAnywhere)
	bool bRequiresDownOrGodLock = false;

	// ヒット時にスポーンするVFX(毎ヒット。未設定ならスキップ)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> HitVFX = nullptr;

	// 破壊時に再生するモンタージュ(ひざまずき〜起き上がりまで含む。未設定ならスキップ)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> DestroyMontage = nullptr;

	// 破壊時にスポーンするVFX(未設定ならスキップ)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> DestroyVFX = nullptr;

	// ヒット時の部位揺れ：初速(度/秒)※0で揺れなし
	UPROPERTY(EditAnywhere, Category = "HitShake")
	float HitShakeImpulse = 0.0f;

	// ヒット時の部位揺れ：減衰するまでの秒数
	UPROPERTY(EditAnywhere, Category = "HitShake")
	float HitShakeDuration = 0.3f;

	// 部位光輪設定 (HaloConfig.HaloMeshがnullなら光輪なし)
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	FPartHaloConfig HaloConfig;

	// trueのとき、攻撃中の光輪消去と無敵化の対象外とする (頭部位など
	// 発光はするが消えず通常通り狙える)
	UPROPERTY(EditAnywhere, Category = "PartHalo")
	bool bExcludeFromHaloHide = false;

};

/**
 * 複数部位の連動破壊リアクション
 * MemberPartTagsのすべてが破壊された瞬間にComboMontage(ダウン演出)を再生する。
 * 成立した部位は個別のDestroyMontageをスキップし、この演出が優先される。
 * 復活は連動では行わず、各部位が個別にBreakRegenCooldown秒後に復活する。
 */
USTRUCT(BlueprintType)
struct FPartComboReaction
{
	GENERATED_BODY()

	// このグループに属する部位タグ。すべて破壊されたら発動する
	UPROPERTY(EditAnywhere)
	TArray<FName> MemberPartTags;

	// 発動時に再生するモンタージュ (ダウン演出)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> ComboMontage = nullptr;

	// trueのとき、SA中 (State.Common.SuperArmor)
	// はこの連動リアクションを再生しない(＝片足やられがSA攻撃を上書きしない)
	// ダウン(全足破壊)はfalseのままにして常時再生させる。破壊状態・光輪演出・復活は抑止しない
	// SAは攻撃モンタージュのNotifyで区間付与する
	UPROPERTY(EditAnywhere)
	bool bSkipWhenSuperArmor = false;

	// 連動リアクションのモンタージュ終了時に強制発動する攻撃のインデックス (-1 = なし)
	// ダウン→反撃バラージのように、ダウン明けに特定の攻撃へ確定で移行させる用途
	UPROPERTY(EditAnywhere)
	int32 ForceAttackIndexOnEnd = -1;

};

/**
 * ダウン復帰中の接触ダメージ設定 (ミニオンのUHaloComponent::ApplyHaloTouchDamage相当)。
 * 復帰演出中は光輪を発光させ、アクター中心から半径内の敵対Pawnへ周期的にダメージを与える
 */
USTRUCT(BlueprintType)
struct FBossRecoveryTouch
{
	GENERATED_BODY()

	// 接触ダメージ判定の半径。アクター中心からの水平距離で判定する
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float AoeRadius = 300.0f;

	// 1ヒットあたりのダメージ
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	// 同一対象への再ヒット秒間隔 ※窓が開いている間この間隔で繰り返しヒット
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.05"))
	float RehitInterval = 0.5f;

	// 付与するヒットリアクションタグ
	UPROPERTY(EditAnywhere)
	FGameplayTag HitReactionTag;

	// ヒット時に接触点でスポーンするVFX (未設定ならスキップ)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> TouchEffect = nullptr;

	// TouchEffectのスケール
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float TouchEffectScale = 1.0f;

	// VFX発生同士の最小間隔 (秒)。複数対象を同時/連続ヒットしても、この間隔内は1回しか出さない
	// (バーストの多重起動を防ぐマージン。再ヒット間隔RehitIntervalとは独立)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float TouchEffectMinInterval = 0.2f;

};

/**
 * ボスのフェーズエントリー
 * Phases配列のインデックスがそのままフェーズ番号になる (0 = 初期フェーズ)
 */
USTRUCT(BlueprintType)
struct FBossPhaseEntry
{
	GENERATED_BODY()

	// HPがこの割合を下回ったら次のフェーズへ (0.0 = 最終フェーズ用。遷移しない)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HPThreshold = 0.5f;

	// フェーズ移行時に再生するモンタージュ (未設定なら即移行)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> TransitionMontage = nullptr;

	// BTの分岐条件に使うフェーズタグ。BBキー"CurrentBossPhase"への書き込みと併用
	UPROPERTY(EditAnywhere)
	FGameplayTag PhaseTag;

	// このパーツが破壊されたとき次フェーズへ移行 (NAME_None = 部位トリガーなし)
	// HPThresholdとOR条件: どちらか先に満たされた方でフェーズ移行する
	UPROPERTY(EditAnywhere)
	FName PartTriggerTag;

};

/**
 * ボスの旋回アニメーション設定
 * 角度閾値を超えている間だけクールダウンが減少し、満了時に旋回モンタージュを発火する
 */
USTRUCT(BlueprintType)
struct FBossTurnAnimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Turn90Left   = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Turn90Right  = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Turn180Left  = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Turn180Right = nullptr;

	// この角度以上のとき、クールダウンが減少し始める(度)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float Threshold90 = 60.0f;

	// この角度以上で180系モンタージュを使用(度)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float Threshold180 = 130.0f;

	// 旋回後の次の旋回までの待機秒数
	UPROPERTY(EditAnywhere)
	float Cooldown = 6.0f;

	// 旋回中の回転速度(度/秒)。モンタージュの見た目に合わせて調整する
	UPROPERTY(EditAnywhere)
	float RotationRate = 180.0f;

};

/**
 * Ph1足場ジャンプ移動の設定
 * ジャンプモーションは見た目用 (位置はボス側が放物線で駆動する)
 */
USTRUCT(BlueprintType)
struct FBossPlatformJumpSettings
{
	GENERATED_BODY()

	// ジャンプの見た目モンタージュ(踏ん張り→滞空→落下→着地)
	// 踏ん張り終わりのAnimNotify_AttackEvent
	// (LaunchEventTag)で放物線移動が始まる
	// 滞空/落下はループにすると尺ズレに強い ※nullなら踏ん張りを待たず即離陸(旧挙動)
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	TObjectPtr<UAnimMontage> JumpMontage = nullptr;

	// 離陸の合図となるAnimNotifyイベントタグ (踏ん張り終わりに配置)
	// 受信した瞬間に放物線移動を開始する。無効タグorモンタージュ未設定なら即離陸
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	FGameplayTag LaunchEventTag;

	// 着地時にジャンプするモンタージュセクション名 (落下→着地の同期)
	// NAME_Noneなら強制ジャンプしない (モンタージュの自然再生に任せる)
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	FName LandSectionName = NAME_None;

	// 離陸から着地までの放物線移動秒数 ※滞空+落下の見た目尺に合わせて調整
	UPROPERTY(EditAnywhere, Category = "PlatformJump", meta = (ClampMin = "0.05"))
	float JumpDuration = 0.8f;

	// 踏ん張り中に離陸合図が来なかった場合のフェイルセーフ秒数 (これを超えたら強制離陸)
	UPROPERTY(EditAnywhere, Category = "PlatformJump", meta = (ClampMin = "0.1"))
	float WindUpTimeout = 1.5f;

	// 放物線の頂点の高さ
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	float ArcHeight = 400.0f;

	// ジャンプ中に着地先へヨーを向けるか (false = 現状の向きを維持)
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	bool bFaceTarget = true;

	// 着地スウィープの上端 (着地XYから上へcm)
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	float LandTraceUp = 200.0f;

	// 着地スウィープの下方向距離
	UPROPERTY(EditAnywhere, Category = "PlatformJump")
	float LandTraceDown = 600.0f;

};

/**
 * ボスキャラクターデータアセット
 */
UCLASS()
class PRJ_TIDE_P0_API UBossDataAsset : public UEnemyDataAsset
{
	GENERATED_BODY()

public:

	// 部位破壊設定
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	TArray<FPartEntry> Parts;

	// 複数部位の連動破壊リアクション設定
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	TArray<FPartComboReaction> PartComboReactions;

	// ダウン復帰中の接触ダメージ設定 (反撃バラージ実行中に有効化される)
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	FBossRecoveryTouch RecoveryTouch;

	// フェーズ設定 (インデックス順に定義。最後の要素のHPThresholdは無視される)
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	TArray<FBossPhaseEntry> Phases;

	// 旋回アニメーション設定
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	FBossTurnAnimSettings TurnAnim;

	// Ph1足場ジャンプ移動設定
	UPROPERTY(EditAnywhere, Category = "Tide|Boss")
	FBossPlatformJumpSettings PlatformJump;

};
