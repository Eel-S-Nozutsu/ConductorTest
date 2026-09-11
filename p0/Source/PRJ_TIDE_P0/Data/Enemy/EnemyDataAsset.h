// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Enemy/AttackExecution/TideAttackExecution.h"
#include "PRJ_TIDE_P0/Data/Enemy/DamageReactionTables.h"
#include "PRJ_TIDE_P0/Data/Combat/DeathRagdollTypes.h"
#include "PRJ_TIDE_P0/Utilities/HaloShake.h"
#include "EnemyDataAsset.generated.h"

/**
 * 攻撃の距離グループ (ビットマスク)。実数レンジの代わりに近・中・遠で分類する。
 * enum値はビットインデックスとして扱う (Near=bit0, Mid=bit1, Far=bit2)
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class EAttackRangeGroup : uint8
{
	Near UMETA(DisplayName = "近"),
	Mid  UMETA(DisplayName = "中"),
	Far  UMETA(DisplayName = "遠"),
};

/**
 * 死亡ラグドールの吹き飛び回転。全ボディへ同一速度を入れるだけだと相対速度がゼロになり、
 * 直立ポーズのまま平行移動してしまうため、ここで角運動量と体の折れを作る
 */
USTRUCT(BlueprintType)
struct FDeathRagdollSpin
{
	GENERATED_BODY()

	// 回転の作り方。きりもみ = 進行方向を軸にロール、前転/横転 = 進行方向と直交する水平軸で回る
	UPROPERTY(EditAnywhere, meta = (DisplayName = "回転の種類"))
	EDeathRagdollSpinMode Mode = EDeathRagdollSpinMode::Corkscrew;

	// 回転速度 (度/秒)。360で1秒1回転
	UPROPERTY(EditAnywhere, meta = (DisplayName = "回転速度", ClampMin = "0.0"))
	float SpeedDegrees = 3000.0f;

	// 回転速度のばらつき (0 = 毎回同じ, 0.5 = ±50%)。死体の回り方が揃って見えるのを防ぐ
	UPROPERTY(EditAnywhere, meta = (DisplayName = "回転のばらつき", ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedVariance = 0.35f;

	// 回転の向きを毎回ランダムに反転する。OFFなら常に同じ向きに回る
	UPROPERTY(EditAnywhere, meta = (DisplayName = "回転の向きをランダム化"))
	bool bRandomizeDirection = true;

	// 追加速度を入れるボーン。重心から外れた位置に効かせて上体から持っていく (None = 追加なし)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "追加インパルスのボーン"))
	FName ImpulseBone = TEXT("spine_02");

	// 上記ボーンへ吹き飛び速度の何倍を上乗せするか。大きいほど体が大きく折れる
	UPROPERTY(EditAnywhere, meta = (DisplayName = "追加インパルス倍率", ClampMin = "0.0"))
	float ImpulseScale = 0.6f;

};

/**
 * 攻撃エントリー。UEnemyDataAssetのインライン配列、またはDataTableの行として使う
 */
USTRUCT(BlueprintType)
struct FAttackEntry : public FTableRowBase
{
	GENERATED_BODY()

	// DataTableの行名 (= 攻撃名)
	// Initialize時に行名から設定される実行時専用フィールド
	FName RowName;

	// OFFで抽選から外して発動しなくする (技の個別ON/OFF)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "有効"))
	bool bEnabled = true;

	// この攻撃のダメージ。負値 = 未設定 (弾/BP既定値を使う)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "ダメージ"))
	float Damage = 10.0f;

	// この攻撃が被弾側へ与えるヒットリアクション。空 = 弾は自前タグ / 近接は無し
	UPROPERTY(EditAnywhere, meta = (DisplayName = "ヒットリアクション", Categories = "HitReaction"))
	FGameplayTag HitReactionTag = TAG_HitReaction_Knockback_S;

	// この攻撃で出す弾が、発射元スポナーの範囲 (InnerVolume = テリトリー)
	// を超えたらフェードして消える。PCがエリア外へ逃げたとき、
	// 弾が場外まで飛んで理不尽に当たるのを防ぐ
	// スポナー管理下 (テリトリー半径 > 0) の敵の弾でのみ効く。直置き敵の弾は対象外
	UPROPERTY(EditAnywhere, meta = (DisplayName = "スポナー範囲外で弾をフェード"))
	bool bFadeProjectilesOutsideSpawnerRange = false;

	// 発動する距離グループ (近/中/遠、複数選択可)。0 = どの距離でも発動しない
	UPROPERTY(EditAnywhere, meta = (DisplayName = "距離グループ", Bitmask, BitmaskEnum = "/Script/PRJ_TIDE_P0.EAttackRangeGroup"))
	int32 RangeGroups = 0;

	// この角度以内にターゲットがいる場合のみ発動 (0 = 無制限)。0度=正面180度=真後ろ
	UPROPERTY(EditAnywhere, meta = (DisplayName = "正面側角度(以内)", ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAngleToTarget = 0.0f;

	// この角度以上ターゲットが正面からずれている場合のみ発動 (0 = 無制限)。例135 = 背後専用
	UPROPERTY(EditAnywhere, meta = (DisplayName = "背面側角度(以上)", ClampMin = "0.0", ClampMax = "180.0"))
	float MinAngleToTarget = 0.0f;

	// ONで、この攻撃の本体前にPCの方へ向き直るモーション
	// (AISettingsの「向き直りモーション」)を1回挟む
	// 既に正面付近 (AISettingsの向き直り角度以内) なら向き直らず即発動する。遠距離タイプ向け
	UPROPERTY(EditAnywhere, meta = (DisplayName = "攻撃前に向き直る"))
	bool bTurnToTargetBeforeAttack = false;

	// ターゲットへの上下角がこの角度以内のみ発動 (0 = 無制限)。真下/真上の除外用
	// 60度で ±60度超を除外
	UPROPERTY(EditAnywhere, meta = (DisplayName = "上下角度(以内)", ClampMin = "0.0", ClampMax = "90.0"))
	float MaxPitchToTarget = 0.0f;

	// プレイヤーカメラに連続可視だった時間がこの秒数以上のみ発動 (0 = 無効)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "カメラ可視時間(以上)", ClampMin = "0.0"))
	float MinVisibleToPlayerCameraTime = 0.0f;

	// クールダウン秒数
	UPROPERTY(EditAnywhere, meta = (DisplayName = "クールダウン"))
	float Cooldown = 3.0f;

	// 同名グループの攻撃はCDを共有する (空 = 共有しない)
	// どれか1つを撃つとグループ全員のCDが同時に始まる。1つの攻撃を人数違い等で複数行に
	// 分けたとき、片方を撃った直後にもう片方が撃ててしまうのを防ぐ
	UPROPERTY(EditAnywhere, meta = (DisplayName = "CD共有グループ"))
	FName CooldownGroup;

	// 複数の有効な攻撃がある場合の抽選重み
	UPROPERTY(EditAnywhere, meta = (DisplayName = "抽選重み"))
	float Weight = 1.0f;

	// 抽選優先度。高いPriorityがあればそのティアだけで抽選する (好機攻撃を優先)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "優先度"))
	int32 Priority = 0;

	// グローバルCDを無視して抽選対象にする (個別Cooldownは有効のまま)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "グローバルCD無視"))
	bool bIgnoreGlobalCooldown = false;

	// 同時攻撃トークン (AIDirector) を無視して発動する (好機攻撃用)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "攻撃トークン無視"))
	bool bIgnoreAttackToken = false;

	// トークンを握り続ける秒数(-1=攻撃終了まで)
	// ※持続攻撃は終了まで持つと他の敵が手出しできず不自然
	// コミット区間(初弾まで)の秒数を推奨
	UPROPERTY(EditAnywhere, meta = (DisplayName = "トークン保持時間", ClampMin = "-1.0"))
	float TokenHoldTime = 4.0f;

	// 攻撃実行ロジック。UTideAttackExecutionのサブクラス (C++/BP)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "実行クラス"))
	TSubclassOf<UTideAttackExecution> ExecutionClass;

	// 使用可能フェーズ番号 (-1 = 全フェーズ有効。雑魚敵は設定不要)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ番号"))
	int32 PhaseIndex = -1;

	// このタグの被弾直後に距離無視で即発動する (空 = 通常攻撃として抽選)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "反撃トリガータグ"))
	FGameplayTag CounterTriggerTag;

	// ONで通常抽選から除外し、ForceAttackIndex経由でのみ発動する
	// 協調攻撃の協力者役など、リーダーから指名されて動く攻撃に使う
	UPROPERTY(EditAnywhere, meta = (DisplayName = "指名専用(抽選除外)"))
	bool bCoordinatedOnly = false;

	// これらのタグのいずれかが敵に付いているとき抽選から除外する (例: State.Enemy.Guard)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "除外ステートタグ"))
	FGameplayTagContainer BlockedStateTags;

};

/**
 * 攻撃抽選の距離グループ境界プロフィール (DataTable行型)。
 * 近/中/遠の境界を1か所で定義し、各敵がFDataTableRowHandleで行を選んで参照する
 */
USTRUCT(BlueprintType)
struct FAttackRangeProfile : public FTableRowBase
{
	GENERATED_BODY()

	// これ未満を「近」とする
	UPROPERTY(EditAnywhere, meta = (DisplayName = "近/中 境界"))
	float NearMidBoundary = 200.0f;

	// これ未満を「中」、以上を「遠」とする
	UPROPERTY(EditAnywhere, meta = (DisplayName = "中/遠 境界"))
	float MidFarBoundary = 400.0f;

};

UENUM(BlueprintType)
enum class EEnemySpeedType : uint8
{
	Walk   UMETA(DisplayName = "Walk"),
	Strafe UMETA(DisplayName = "Strafe"),
	Run    UMETA(DisplayName = "Run"),
};

/**
 * 敵AI設定
 */
USTRUCT(BlueprintType)
struct FEnemyAISettings
{
	GENERATED_BODY()

	// ストレイフの目標距離 (EQSスコア計算の基準)
	UPROPERTY(EditAnywhere)
	float PreferredRange = 200.0f;

	// EQS距離フィルターの下限 (これより近い点は除外)
	UPROPERTY(EditAnywhere)
	float MinEngageRange = 100.0f;

	// EQS距離フィルターの上限 (これより遠い点は除外)
	UPROPERTY(EditAnywhere)
	float MaxEngageRange = 400.0f;

	// ストレイフで移動したい距離の目標値 (EQSスコアがこの距離でピークになる)
	UPROPERTY(EditAnywhere)
	float PreferredMoveDistance = 300.0f;

	// 視線チェック用の高さオフセット (移動先の地面から上へトレースする)
	UPROPERTY(EditAnywhere)
	float EyeHeightOffset = 150.0f;

	// 接敵と判定する距離。EQSのMaxEngageRangeとは独立
	UPROPERTY(EditAnywhere)
	float EngageRangeThreshold = 600.0f;

	// 接敵解除の上乗せ距離。EngageRangeThresholdで接敵し、
	// +この値を超えて離れるまで接敵を維持する
	// 境界上でアプローチとストレイフがパタつくのを防ぐ (0 = ヒステリシスなし)
	UPROPERTY(EditAnywhere)
	float EngageRangeExitMargin = 50.0f;

	// 適正レンジ維持 (後退) で保ちたい距離。ターゲットがこの距離より近づくと後退する
	// ストレイフのPreferredRangeとは独立。-1 = 無効 (この敵は後退しない)
	// 往復防止の解除ヒステリシスはGetKeepReleaseDistance() が内包する
	UPROPERTY(EditAnywhere)
	float KeepDistance = -1.0f;

	// キープ帯の広さ。KeepDistance〜KeepDistance+KeepBandを「保つ帯
	// (デッドゾーン)」とし、これより遠いときだけ接近する。この帯の中では移動せず攻撃/待機する
	// KeepDistance有効時のみ意味を持つ
	UPROPERTY(EditAnywhere)
	float KeepBand = 400.0f;

	// 後退の解除ライン。ここまで離れたらIsTooCloseを解除し、後退先もこの距離を狙う
	// KeepDistanceに往復防止マージンを上乗せした値 (KeepDistance無効なら
	// -1)。判定と後退先で同じ値を使うため、導出をここ1か所に集約する
	float GetKeepReleaseDistance() const
	{
		return KeepDistance < 0.0f ? -1.0f : KeepDistance + GetKeepHysteresis();
	}

	// 接近を開始する距離 (KeepDistance+KeepBand)
	// これより遠いとIsTooFarが接近させる (KeepDistance無効なら -1)
	float GetKeepFarThreshold() const
	{
		return KeepDistance < 0.0f ? -1.0f : KeepDistance + FMath::Max(KeepBand, 0.0f);
	}

	// 往復防止ヒステリシス幅。IsTooClose/IsTooFarの発動と解除の閾値差に共通で使う
	float GetKeepHysteresis() const
	{
		return FMath::Max(KeepDistance * 0.1f, 30.0f);
	}

	// 攻撃終了後の最低待機時間。緩急をつけるグローバルCD (0 = 無効)
	UPROPERTY(EditAnywhere)
	float GlobalAttackCooldown = 3.0f;

	// 攻撃前にPCの方へ向き直るモーション。攻撃側 (FAttackEntryの「攻撃前に向き直る」)
	// がONで、かつPCが正面からTurnToTargetThreshold以上ズレているときだけ、
	// 攻撃本体の前に1回再生する
	// 実際の回頭はこのモンタージュに置いた
	// 「ターゲット方向へ回転」Notifyが駆動する (未設定=向き直らない)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> TurnToTargetMontage = nullptr;

	// 向き直りを挟む角度しきい値 (度)。PCがこの角度以内なら向き直りをスキップして即攻撃する (0
	// = 常に向き直る)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float TurnToTargetThreshold = 20.0f;

	// 攻撃抽選の距離グループ境界プロフィール。未設定時は近=200 / 遠=400の既定を使う
	UPROPERTY(EditAnywhere, meta = (RowType = "/Script/PRJ_TIDE_P0.AttackRangeProfile"))
	FDataTableRowHandle RangeProfile;

	// 歩き速度
	UPROPERTY(EditAnywhere)
	float WalkSpeed = 100.0f;

	// ストレイフ速度
	UPROPERTY(EditAnywhere)
	float StrafeSpeed = 260.0f;

	// 走り速度
	UPROPERTY(EditAnywhere)
	float RunSpeed = 300.0f;

};

/**
 * 敵の検知(感知)設定。UEnemyPerceptionComponentの視覚センスに適用される。
 */
USTRUCT(BlueprintType)
struct FEnemyPerceptionSettings
{
	GENERATED_BODY()

	// 視覚半径。これに入り視野角・LOSが通れば検知ゲージが溜まりはじめる
	// ※スポナー配下の敵は在圏のみで検知するため視覚半径・視野角は検知に効かない
	UPROPERTY(EditAnywhere)
	float SightRadius = 3000.0f;

	// 水平視野角 (度)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionAngle = 180.0f;

	// 検知ゲージ: 交戦圏(スポナーのOuterVolume)にプレイヤーが留まってから
	// 戦闘に入るまでの秒数。0で即時(ゲージ無効)
	// ※スポナー配下は視認を条件にしない。手置き敵は視野に捉え続けた時間で溜まる
	// ※InnerVolume侵入によるエンカウント起動はゲージを介さず即時
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float DetectionFillSeconds = 1.0f;

	// 検知が切れている間にゲージが満タンから0へ戻るまでの秒数。0で減衰しない
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float DetectionDecaySeconds = 2.0f;

};

/**
 * ボスのステップ発火方式
 *
 * Pressure   : 被弾回数が閾値に達したら後退
 * Interval   : 一定間隔で後退
 * Proximity  : プレイヤーが近接閾値に入ったとき後退(未実装)
 * PostAttack : 攻撃終了のリカバリー中にリポジション(未実装)
 */
UENUM(BlueprintType)
enum class EBossStepTriggerPattern : uint8
{
	Pressure   UMETA(DisplayName = "Pressure"),
	Interval   UMETA(DisplayName = "Interval"),
	Proximity  UMETA(DisplayName = "Proximity (not impl.)"),
	PostAttack UMETA(DisplayName = "PostAttack (not impl.)"),
};

/**
 * ストレイフ中に差し込むステップアニメーション設定
 */
USTRUCT(BlueprintType)
struct FStepAnimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Forward  = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Backward = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Right    = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Left     = nullptr;

	// ステップ再生間隔の下限・上限秒数
	UPROPERTY(EditAnywhere) float MinInterval = 1.5f;
	UPROPERTY(EditAnywhere) float MaxInterval = 3.0f;

	// 移動速度がこの値未満の場合はスキップ (静止中は再生しない)
	UPROPERTY(EditAnywhere) float MinSpeedThreshold = 0.15f;

	// ステップ先のNavMeshチェック距離。0 = チェックしない
	UPROPERTY(EditAnywhere) float NavCheckDistance = 650.0f;

	// ボスのステップ発火方式
	UPROPERTY(EditAnywhere) EBossStepTriggerPattern BossStepTrigger = EBossStepTriggerPattern::Pressure;

	// Pressureモード: 何回ヒットを受けたらステップを発火するか (ボス専用)
	UPROPERTY(EditAnywhere) int32 StepPressureThreshold = 3;

	// 予知回避の発動確率 (0.0=しない1.0=必ずする)
	UPROPERTY(EditAnywhere) float PredictiveDodgeSuccessRate = 0.5f;

	// 回避の最小秒間隔
	UPROPERTY(EditAnywhere) float DodgeCooldown = 2.0f;

	// 予知回避成功後に強制する反撃攻撃インデックス (-1 = 指定なし)
	UPROPERTY(EditAnywhere, meta = (DisplayName = "予知回避反撃インデックス"))
	int32 PredictiveDodgeCounterAttackIndex = -1;

};

// FBlowbackRecoveryEntry /
// FBlowbackAnimSequenceはDamageReactionTables.hへ移動
// (テーブル行から同じ型を組み立てて共通のドライバへ渡すため)

/**
 * 竜巻 (風) で巻き上げられたときのリアクションモーション。
 * Startがあれば打ち上げ開始で1回再生し、上昇~滞空はAirLoop、落下はFallLoopをループ再生し
 * (1サイクル素材を着地まで手動再トリガ)、着地でLand、続けてRecoveryを再生してからAIを再開する。
 */
USTRUCT(BlueprintType)
struct FWindReactionAnim
{
	GENERATED_BODY()

	// 打ち上げ開始。設定時はAirLoopの前に1回だけ再生する
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Start = nullptr;

	// 打ち上げ初速 (上向き)。>0で「物理打ち上げモード」:
	// LaunchCharacterで撃ち出し重力で放物線を描く。0のときは従来の竜巻 (持続揚力) 方式
	UPROPERTY(EditAnywhere) float LaunchUpForce = 0.0f;

	// 上昇開始~滞空中のループ
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> AirLoop = nullptr;

	// 落下中のループ
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> FallLoop = nullptr;

	// 着地
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Land = nullptr;

	// 着地後の起き上がり
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Recovery = nullptr;

	// true: 竜巻で浮かない (ボス等)。スリップダメージは受けるが巻き上げられない
	UPROPERTY(EditAnywhere) bool bAnchored = false;

	// 浮かない敵が竜巻 (大) に当たったときののけぞり。SAを貫通して直接再生し、竜巻内はループする
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> LargeWindFlinch = nullptr;

	// 最後に竜巻 (大) のスリップを受けてからこの秒数を超えたら、のけぞりループを終了する
	// 竜巻のスリップ間隔より長く設定する
	UPROPERTY(EditAnywhere) float AnchoredFlinchHoldTime = 0.7f;

};

/**
 * 敵キャラクターデータアセット
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyDataAsset : public UTideCharacterDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "AI")
	FEnemyAISettings AISettings;

	// 検知(感知)設定。視覚センスの半径・視野角
	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	FEnemyPerceptionSettings Perception;

	// 攻撃定義のDataTable (行型 = FAttackEntry)。行名が攻撃名になる
	UPROPERTY(EditAnywhere, Category = "Attacks")
	TObjectPtr<UDataTable> AttackTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Step")
	FStepAnimSettings StepAnim;

	// 死亡ポーズ。1Fモンタージュを想定 (Blend In/Outは0にすること)
	// 未設定 = 直前のポーズのまま物理化
	UPROPERTY(EditAnywhere, Category = "Death")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	// 上記を再生してからラグドール化するまでの保持時間 (秒)
	// Montage_Playはその場でボーンを動かさず次のアニメ評価で反映されるため、
	// 0だと再生前のポーズで物理化してしまう。少し持たせると崩れ落ちる前の「溜め」になる
	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DeathPoseHoldTime = 0.08f;

	// 非ノックバック死亡時に攻撃方向へ吹き飛ぶ速度
	UPROPERTY(EditAnywhere, Category = "Death")
	float DeathLaunchForce = 3000.0f;

	// 吹き飛びラグドールの回り方 (きりもみ/前転など)
	UPROPERTY(EditAnywhere, Category = "Death")
	FDeathRagdollSpin DeathSpin;

	// フェードアウト開始と同時に再生する爆発Niagara (未設定 = 再生しない)
	UPROPERTY(EditAnywhere, Category = "Death")
	TObjectPtr<class UNiagaraSystem> DeathExplosionEffect = nullptr;

	UPROPERTY(EditAnywhere, Category = "Death")
	float DeathExplosionScale = 1.0f;

	// 死亡ラグドール中に敵・壊れ物へ衝突したときのダメージ (0=ダメージなし)
	UPROPERTY(EditAnywhere, Category = "Death")
	float DeathRagdollCollisionDamage = 20.0f;
	// 死亡ラグドール衝突時に被弾者へ適用するヒットリアクションタグ
	UPROPERTY(EditAnywhere, Category = "Death")
	FGameplayTag DeathRagdollCollisionHitReactionTag;

	// 吹き飛び/叩きつけ/打ち上げ
	// /巻き上げのモーション定義はHitReactionTableのSequence行へ移行済み
	// (BlowbackAnim / SmashDownAnim /BlowupAnim /
	// WindReactionのモーション列は削除)

	// 竜巻 (風) の挙動設定。モーションはテーブルへ移したが、
	// 以下は「敵の性質」なのでDAに残す:bAnchored (巻き上げ対象か) /
	// LargeWindFlinch・AnchoredFlinchHoldTime
	// (浮かない敵ののけぞりループ)
	UPROPERTY(EditAnywhere, Category = "Wind")
	FWindReactionAnim WindReaction;

	// 光輪のスタティックメッシュ。AEnemyCharacterがOnConstructionでこのメッシ
	// ュから光輪コンポーネントを構成する (エディタで見える)。BPへダミーメッシュを置く必要はない
	UPROPERTY(EditAnywhere, Category = "Halo")
	TObjectPtr<class UStaticMesh> HaloMesh = nullptr;

	// 光輪の待機ソケット (非ガード時)
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName HaloBackSocketName = TEXT("halo_back");

	// 光輪の待機ソケット (ガード中)
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	FName HaloWaistSocketName = TEXT("halo_waist");

	// 通常(非ガード)時の光輪スケール。BPのメッシュスケールではなくこの値が適用される
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloNormalScale = 1.0f;

	// ガード中の光輪スケール
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	float HaloGuardScale = 1.5f;

	// ガード自動解除までの最大秒数 ※0で時間制限なし
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	float GuardAutoReleaseDuration = 5.0f;

	// ガード解除時のディザフェード秒数
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloDitherTime = 0.5f;

	// ディザマテリアルパラメータ名
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName HaloDitherParamName = TEXT("DitherAlpha");

	// ガード解除時のVFX (ヒット位置にスポーン)
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	TObjectPtr<class UNiagaraSystem> HaloGuardBreakEffect;

	// ガード解除VFXのスケール
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	FVector HaloGuardBreakEffectScale = FVector(1.0f);

	// 攻撃でガードを割った時のジオメトリコレクション破壊アセット
	UPROPERTY(EditAnywhere, Category = "Halo")
	TObjectPtr<class UGeometryCollection> HaloBreakGeometryCollection;

	// 破砕後、破片が消えるまでの秒数
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakReformDelay = 2.0f;

	// 破壊後、光輪が再生するまでのクールタイム。この間は無防備 (本体肉質) で光輪技も使えない
	// 0 = 破片が消えるのと同時 (HaloBreakReformDelay) に復帰する従来挙動
	UPROPERTY(EditAnywhere, Category = "Halo", meta = (ClampMin = "0.0"))
	float HaloBreakRegenCooldown = 8.0f;

	// 光輪技が中断され、光輪を飛ばしたまま取り残されたときに再生するまでの技クールタイム
	// この間は無防備。0 = 中断と同時に手元へ戻す従来挙動
	UPROPERTY(EditAnywhere, Category = "Halo|Throw", meta = (ClampMin = "0.0"))
	float HaloThrowRegenCooldown = 3.0f;

	// GCクラスターを砕くStrainフィールドの強さ
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakStrainMagnitude = 1000000.0f;

	// Strain/Scatterフィールドの作用半径
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakFieldRadius = 200.0f;

	// 破片に加えるラジアル散布力の強さ
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakScatterForce = 200000.0f;

	// 破片のフェードアウト秒数 ※0でフェードなし
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakFadeDuration = 0.5f;

	// 光輪が健在な間ずっと胴体 (本体メッシュ) へ出すフレネルの強度
	// Custom Primitive Data Index[0]
	// 破壊/投擲で光輪が手元にない間は0へ (無防備の見た目)。0で常に無効
	UPROPERTY(EditAnywhere, Category = "Halo")
	float HaloBreakFresnelIntensity = 1.0f;

	// 胴体フレネルの色。Custom Primitive Data Index[1..3] =
	// R/G/B。既定は黄色 (エネルギー纏い)
	UPROPERTY(EditAnywhere, Category = "Halo")
	FLinearColor HaloBreakFresnelColor = FLinearColor(1.0f, 0.85f, 0.1f);

	// フレネルの点灯/消灯にかけるフェード秒数 ※0で即時
	UPROPERTY(EditAnywhere, Category = "Halo", meta = (ClampMin = "0.0"))
	float HaloBreakFresnelFadeOutTime = 1.0f;

	// 光輪ひびのマテリアルパラメータ名 (0.0=正常1.0=ひびあり)
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName HaloCrackParamName = TEXT("Crack");

	// PCの攻撃がガード光輪にヒットしたが壊れなかったときの3軸回転ブレ
	UPROPERTY(EditAnywhere, Category = "Halo")
	FHaloHitShakeParams HaloHitShake;

	// ガード中の進入防止バリア球の半径
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	float HaloBarrierRadius = 80.0f;

	// タッチからダメージ発火までの遅延秒数
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchDamageDelay = 2.0f;

	// VFXをダメージより何秒早く再生するか (0 = 同時)
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchVfxOffset = 0.2f;

	// ダメージ判定を張り続ける秒数。0で発火時の一瞬のみ
	// VFXが「ダメージを出していそうに見える」尺に合わせる
	UPROPERTY(EditAnywhere, Category = "Halo|Touch", meta = (ClampMin = "0.0"))
	float HaloTouchDamageDuration = 0.3f;

	// 発火時のAOEダメージ量
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchDamage = 20.0f;

	// 発火時のAOE半径 (光輪半径より大きく設定する)
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchAoeRadius = 200.0f;

	// 接触トリガーの半径。この球へ入るとダメージ予告 (フリッカー/VFX) が始まる
	UPROPERTY(EditAnywhere, Category = "Halo|Touch", meta = (ClampMin = "0.0"))
	float HaloTouchTriggerRadius = 200.0f;

	// 発火後の再受付クールダウン
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchCooldown = 1.5f;

	// 発火時のNiagaraエフェクト
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	TObjectPtr<UNiagaraSystem> HaloTouchEffect;

	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	float HaloTouchEffectScale = 1.0f;

	// タッチ中の点滅マテリアルパラメータ名 (0.0=消灯1.0=点灯)
	UPROPERTY(EditAnywhere, Category = "Halo|Touch")
	FName HaloTouchGlowParamName = TEXT("TouchGlow");

	// 攻撃時の明るさマテリアルパラメータ名 (0.0=暗め:ガード・通常1.0=明るめ:攻撃)
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName HaloAttackGlowParamName = TEXT("AttackGlow");

	// 背中待機中に背後から攻撃されると破壊: 後方コーン水平半角 (度)。90=後ろ半球
	UPROPERTY(EditAnywhere, Category = "Halo|BackDestroy")
	float HaloBackDetectHalfAngleDeg = 90.0f;

	// 背中待機中に背後から攻撃されると破壊: 攻撃者と敵の許容高低差
	UPROPERTY(EditAnywhere, Category = "Halo|BackDestroy")
	float HaloBackDetectMaxHeightDiff = 150.0f;

	// 背中光輪破壊時に通常リアクションを上書きするタグ (未設定=上書きしない)
	UPROPERTY(EditAnywhere, Category = "Halo|BackDestroy")
	FGameplayTag HaloBackBreakReactionTag;

	// 背中光輪ヒビ時 (破壊に至らない被弾) に通常リアクションを上書きする小リアクション
	// (未設定=上書きしない)
	UPROPERTY(EditAnywhere, Category = "Halo|BackDestroy")
	FGameplayTag HaloBackCrackReactionTag;

	// trueで背中光輪を常にヒビ割れ状態にする (初期・再生後ともヒビのまま)
	// ヒビ中は背後からの一撃 (軽ギア/ギア無しでも) で即破壊される = 常に脆い背中光輪
	UPROPERTY(EditAnywhere, Category = "Halo|BackDestroy")
	bool bHaloBackAlwaysCracked = false;

	// 光輪ガード破壊時に通常リアクションを上書きするタグ (未設定=上書きしない)
	UPROPERTY(EditAnywhere, Category = "Halo|Guard")
	FGameplayTag HaloGuardBreakReactionTag;

	// 光輪を技で飛ばして手元に無い＝無防備の間、本体被弾に掛かるダメージ倍率。1.0=変化なし
	// (単体ザコ用。ボスは部位ごとのHaloSystemComponentユニットで肉質を管理する)
	UPROPERTY(EditAnywhere, Category = "Halo", meta = (ClampMin = "0.0"))
	float HaloAwayBodyDamageMultiplier = 1.5f;


	// この敵で硬化装鋼を使うか。ONの間はState.Enemy.Armamentで被ダメージ軽減を行う
	UPROPERTY(EditAnywhere, Category = "Armament")
	bool bUseArmament = false;

	// BeginPlay時点で硬化装鋼を有効にして開始するか
	UPROPERTY(EditAnywhere, Category = "Armament", meta = (EditCondition = "bUseArmament"))
	bool bStartWithArmamentActive = false;

	// 硬化装鋼中の被ダメージ倍率。0.0666... で通常の1/15ダメージになる
	UPROPERTY(EditAnywhere, Category = "Armament", meta = (EditCondition = "bUseArmament", ClampMin = "0.0", ClampMax = "1.0"))
	float ArmamentDamageMultiplier = 0.06666667f;

	// 硬化が解除されて攻撃が通る状態を表すマテリアルfloatパラメータ名 (硬化中=0, 解除中=1)
	UPROPERTY(EditAnywhere, Category = "Armament", meta = (EditCondition = "bUseArmament"))
	FName ArmamentVisualParamName = TEXT("ArmamentOpen");

	// このタイプの胴体着色 (Tint)。白 = 無着色。色でどの技を出す個体かを覚えさせる用
	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor BodyColor = FLinearColor::White;

	// BodyColorを流し込む胴体マテリアルのVectorパラメータ名。Noneなら色を適用しない
	UPROPERTY(EditAnywhere, Category = "Appearance")
	FName BodyColorParamName = TEXT("BodyColor");

	// ヒットリアクション定義のDataTable (行型 = FHitReactionRow)
	// 行名がリアクション名になる。敵のリアクション(単発・多段・巻き上げ)はすべてここから解決する
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TObjectPtr<UDataTable> HitReactionTable = nullptr;

	// 受信リアクションxヒット方向 → 変換後リアクション
	// 行キー: {ReactionLeaf}_{Direction} (例
	// "KnockbackS_Back")
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TObjectPtr<UDataTable> ReactionConversionTable = nullptr;

	// ヒット方向ごとのダメージ倍率。行キー: "Front" / "Back"
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TObjectPtr<UDataTable> DirectionMultiplierTable = nullptr;

	// 部位ごとのダメージ倍率。行キー: HitPartTagのリーフ名 (例 "LeftLeg")
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TObjectPtr<UDataTable> PartMultiplierTable = nullptr;

};
