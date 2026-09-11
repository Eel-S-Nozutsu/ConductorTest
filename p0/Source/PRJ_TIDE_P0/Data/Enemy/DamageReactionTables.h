// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "DamageReactionTables.generated.h"

class UAnimMontage;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EHitDirection : uint8
{
	Front,
	Back,
	Any,	// 方向を問わずマッチ(Front/Backの厳密一致より優先度が低い)
};

/**
 * リアクション変換テーブル
 *
 * InputReactionTag + HitDirection + Partの組み合わせでOutputReactionTagを決定する。
 * PartがNAME_Noneの行はワイルドカード(部位一致行がなければ採用)。
 * マッチする行がない場合は入力タグをそのまま使用する。
 *
 * 行キーは任意 (例: 連番 "01", "02" など)。
 * EnemyDataAsset::ReactionConversionTableに設定する。
 */
USTRUCT(BlueprintType)
struct FReactionConversionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (Categories = "HitReaction"))
	FGameplayTag InputReactionTag;

	UPROPERTY(EditAnywhere)
	EHitDirection HitDirection = EHitDirection::Any;

	// HitPartTagのリーフ名 (例: "LeftLeg")。未設定 = 部位問わずマッチ
	UPROPERTY(EditAnywhere)
	FName Part = NAME_None;

	UPROPERTY(EditAnywhere, meta = (Categories = "HitReaction"))
	FGameplayTag OutputReactionTag;

};

/**
 * 方向ダメージ倍率
 *
 * 行キー: "Front" / "Back"
 * EnemyDataAsset::DirectionMultiplierTableに設定する。
 */
USTRUCT(BlueprintType)
struct FDirectionMultiplierRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Multiplier = 1.0f;

};

/**
 * 部位ダメージ倍率
 *
 * 行キー: HitPartTagのリーフ名 (例: "LeftLeg", "RightLeg")
 * EnemyDataAsset::PartMultiplierTableに設定する。
 */
USTRUCT(BlueprintType)
struct FPartMultiplierRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Multiplier = 1.0f;

};

/**
 * 起き上がりモンタージュ1件。NavMeshチェックを通す移動方向を持つ
 */
USTRUCT(BlueprintType)
struct FBlowbackRecoveryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Montage = nullptr;

	// アクターローカル移動方向 (X=前後, Y=左右
	// ZeroVector=Navチェックなし・常に候補)
	UPROPERTY(EditAnywhere) FVector2D LocalMoveDir = FVector2D::ZeroVector;

};

/**
 * ふっとびモーション設定
 *
 * 多段リアクションのドライバ (UHitReactionComponent) が受け取る実行時の形。
 * DT行 (FHitReactionRow) からこの形に組み立てて渡す。
 * Loopはモンタージュが1サイクルで終わる前提で、着地まで手動で繰り返す
 */
USTRUCT(BlueprintType)
struct FBlowbackAnimSequence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Start = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Loop  = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UAnimMontage> Land  = nullptr;

	// 起き上がりモンタージュ。NavMeshチェックを通過した候補から前回除外でランダム選択する
	UPROPERTY(EditAnywhere) TArray<FBlowbackRecoveryEntry> Recoveries;

	// RecoveriesのNavMeshチェック距離。0=チェックなし
	UPROPERTY(EditAnywhere) float RecoveryNavCheckDistance = 300.0f;

	// 壁衝突モーション (st-lp-ed)
	// 未設定スロットは通常モンタージュ (Start/Loop/Land) にフォールバック
	UPROPERTY(EditAnywhere, Category = "WallHit") TObjectPtr<UAnimMontage> WallHitStart = nullptr;
	UPROPERTY(EditAnywhere, Category = "WallHit") TObjectPtr<UAnimMontage> WallHitLoop  = nullptr;
	UPROPERTY(EditAnywhere, Category = "WallHit") TObjectPtr<UAnimMontage> WallHitLand  = nullptr;
	// 壁衝突後の起き上がり。空ならRecoveriesにフォールバック
	UPROPERTY(EditAnywhere, Category = "WallHit") TArray<FBlowbackRecoveryEntry> WallHitRecoveries;
	// 壁反射の速度係数 (0=完全吸収, 1=完全弾性)
	UPROPERTY(EditAnywhere, Category = "WallHit") float WallHitBounceRestitution = 0.5f;

	// 空中フェーズ中、モンタージュのroot motion移動を無効化する
	// 垂直移動RMが入った素材で「着地までループ」が成立しない場合にtrue
	// 着地で復帰するためLand/RecoveryのRMは活きる
	UPROPERTY(EditAnywhere, Category = "RootMotion") bool bSuppressAirborneRootMotion = false;

	// 吹き飛び中に敵・壊れ物へ衝突したときのダメージ (0=ダメージなし)
	UPROPERTY(EditAnywhere, Category = "Collision") float CollisionDamage = 20.0f;
	// 吹き飛び衝突時に被弾者へ適用するヒットリアクションタグ
	UPROPERTY(EditAnywhere, Category = "Collision") FGameplayTag CollisionHitReactionTag;

};

/**
 * 起き上がりセット
 *
 * 行キー: セット名 (例: "Zombie_Down_D" / "Halo_Down_U" / "Halo_Down_D")
 * FHitReactionRow::RecoverySet / WallHitRecoverySetからFDataTableRowHandleで参照する。
 * 複数のリアクション行が同じセットを参照できるため、同じ起き上がりを何度も設定しなくてよい。
 */
USTRUCT(BlueprintType)
struct FRecoverySetRow : public FTableRowBase
{
	GENERATED_BODY()

	// 起き上がり候補。NavMeshチェックを通過したものから前回除外でランダム選択する
	UPROPERTY(EditAnywhere)
	TArray<FBlowbackRecoveryEntry> Recoveries;

	// RecoveriesのNavMeshチェック距離。0=チェックなし
	UPROPERTY(EditAnywhere)
	float NavCheckDistance = 300.0f;

};

/**
 * リアクションの種別
 */
UENUM(BlueprintType)
enum class EHitReactionKind : uint8
{
	// 単発モンタージュ (のけぞり等)。Montagesからランダム選択して1本再生する
	Single,
	// 多段シーケンス (吹き飛び・打ち上げ・叩きつけ・風)。開始→空中ループ→着地→起き上がり
	Sequence,
};

/**
 * ヒットリアクション定義
 *
 * 行キー: リアクション名 (例: "Knockback_S" / "Blowoff_S" / "SmashDown" / "Blowup" / "Wind")
 * EnemyDataAsset::HitReactionTableに設定する。
 *
 * Kindで使う列が変わる。Singleは単発列のみ、Sequenceは多段列のみを参照する。
 */
USTRUCT(BlueprintType)
struct FHitReactionRow : public FTableRowBase
{
	GENERATED_BODY()

	// 対応するヒットリアクションタグ
	UPROPERTY(EditAnywhere, meta = (Categories = "HitReaction"))
	FGameplayTag ReactionTag;

	// 単発 / 多段
	UPROPERTY(EditAnywhere)
	EHitReactionKind Kind = EHitReactionKind::Single;

	// ---- 共通 (Kindを問わず使う) ----

	// 被弾エフェクト。SuperArmor中はモンタージュを再生せずこれだけを出すため、
	// 多段 (Kind == Sequence) の行でも設定する
	UPROPERTY(EditAnywhere, Category = "Common")
	TObjectPtr<UNiagaraSystem> HitEffect = nullptr;

	// 吹き飛ばし力。多段シーケンスの開始処理もこの値でLaunchCharacterするため、
	// Blowoff/BlowupなどKind == Sequenceの行でも設定する
	UPROPERTY(EditAnywhere, Category = "Common")
	float LaunchForce = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Common")
	float LaunchUpForce = 0.0f;

	// ---- 単発 (Kind == Single) ----

	// 再生するモンタージュ。未設定ならmontage無し (VFXのみ)
	UPROPERTY(EditAnywhere, Category = "Single", meta = (EditCondition = "Kind == EHitReactionKind::Single", EditConditionHides))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// 被弾時に加害者の方を向く
	UPROPERTY(EditAnywhere, Category = "Single", meta = (EditCondition = "Kind == EHitReactionKind::Single", EditConditionHides))
	bool bFaceAttacker = false;

	// ---- 多段 (Kind == Sequence) ----

	// 開始
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> Start = nullptr;

	// 上昇・滞空ループ。吹き飛びのように空中ループが1種類の場合もここに入れる
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> RiseLoop = nullptr;

	// 落下ループ。未設定ならRiseLoopを流用する (＝空中ループ1種類の挙動)
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> FallLoop = nullptr;

	// 着地
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> Land = nullptr;

	// 起き上がりセット (DT_RecoverySetの行)
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (RowType = "/Script/PRJ_TIDE_P0.RecoverySetRow", EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	FDataTableRowHandle RecoverySet;

	// ---- 壁衝突 (多段のオプション。未設定スロットは通常版へフォールバック) ----

	UPROPERTY(EditAnywhere, Category = "Sequence|WallHit", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> WallHitStart = nullptr;

	UPROPERTY(EditAnywhere, Category = "Sequence|WallHit", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> WallHitLoop = nullptr;

	UPROPERTY(EditAnywhere, Category = "Sequence|WallHit", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	TObjectPtr<UAnimMontage> WallHitLand = nullptr;

	// 壁衝突後の起き上がりセット。未設定ならRecoverySetを使う
	UPROPERTY(EditAnywhere, Category = "Sequence|WallHit", meta = (RowType = "/Script/PRJ_TIDE_P0.RecoverySetRow", EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	FDataTableRowHandle WallHitRecoverySet;

	// 壁反射の速度係数 (0=完全吸収, 1=完全弾性)
	UPROPERTY(EditAnywhere, Category = "Sequence|WallHit", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	float WallHitBounceRestitution = 0.5f;

	// ---- 多段の挙動パラメータ ----

	// 空中フェーズ中、モンタージュのroot motion移動を無効化する
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	bool bSuppressAirborneRootMotion = false;

	// 飛行中に敵・壊れ物へ衝突したときのダメージ (0=ダメージなし)
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	float CollisionDamage = 0.0f;

	// 衝突時に被弾者へ適用するヒットリアクションタグ
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (Categories = "HitReaction", EditCondition = "Kind == EHitReactionKind::Sequence", EditConditionHides))
	FGameplayTag CollisionHitReactionTag;

};
