// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_CommonAttack.generated.h"

// 攻撃ヒットの形状
UENUM(BlueprintType)
enum class EHitShape : uint8
{
	Sphere,
	Capsule,
	Box,
	// 円柱をAngleDegで扇形に切った形状(180で半円)。既存値をずらさないよう末尾に追加する
	Cylinder,
};

// 攻撃ヒット設定
USTRUCT(BlueprintType)
struct FCommonAttackHitSettings
{
	GENERATED_BODY()

public:

	// 形状
	UPROPERTY(EditAnywhere)
	EHitShape Shape = EHitShape::Sphere;

	// オフセット座標
	UPROPERTY(EditAnywhere)
	FVector Offset = FVector::ZeroVector;

	// オフセット回転。Cylinderでは扇の正面(ローカル+X)と円柱の軸(ローカル+Z)をこれで決める
	UPROPERTY(EditAnywhere)
	FRotator Rotation = FRotator::ZeroRotator;

	// スフィア・カプセル・円柱半径
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape==EHitShape::Sphere||Shape==EHitShape::Capsule||Shape==EHitShape::Cylinder"))
	float Radius = 80.0f;

	// ボックスの半辺
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape==EHitShape::Box"))
	FVector BoxExtent = FVector(50.f);

	// カプセル・円柱半長
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape==EHitShape::Capsule||Shape==EHitShape::Cylinder"))
	float HalfHeight = 80.0f;

	// 扇の全角。180=半円、360=全周、90=前方クォーター
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape==EHitShape::Cylinder", ClampMin = "0.0", ClampMax = "360.0"))
	float AngleDeg = 180.0f;

	// 攻撃の定義を表すタグ
	UPROPERTY(EditAnywhere, Category = "HitSettings|Data", meta = (EditCondition = "bUseParameterTable", EditConditionHides, Categories = "AttackType"))
	FGameplayTag AttackTypeTag;

	// データテーブルのパラメータを使用するかどうか
	UPROPERTY(EditAnywhere, Category = "HitSettings|Data")
	bool bUseFallback = false;

	// ダメージ
	UPROPERTY(EditAnywhere, Category = "HitSettings|Fallback", meta = (EditCondition = "bUseFallback", EditConditionHides))
	float Damage = 1.0f;

	// ヒットストップを使用するかどうか
	UPROPERTY(EditAnywhere, Category = "HitSettings|Fallback", meta = (EditCondition = "bUseFallback", EditConditionHides))
	bool bUseHitStop = true;

	// ヒットストップ持続秒数※「テーブルを使わない」かつ「ヒットストップを使う」時だけ表示
	UPROPERTY(EditAnywhere, Category = "HitSettings|Fallback", meta = (EditCondition = "bUseFallback && bUseHitStop", EditConditionHides))
	float HitStopDuration = 0.1f;

	// ヒットストップ時のTimeDilation ※0で完全停止
	UPROPERTY(EditAnywhere, Category = "HitSettings|Fallback", meta = (EditCondition = "bUseFallback && bUseHitStop", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopDilation = 0.1f;

	// ヒットリアクション種別(HitReaction.*)
	// 被弾側がこのタグを参照してのけぞり／吹き飛びを選ぶ。未設定(None)なら上書きせず、
	// OnModifyDamageInfo等で解決された値を維持する
	UPROPERTY(EditAnywhere, Category = "HitSettings|Fallback", meta = (EditCondition = "bUseFallback", EditConditionHides, Categories = "HitReaction"))
	FGameplayTag HitReactionTag;

};

/**
 * 汎用攻撃通知
 */
UCLASS(Blueprintable, meta = (DisplayName = "CommonAttack", Category = "Common"))
class PRJ_TIDE_P0_API UAnimNotifyState_CommonAttack : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

public:

	// ソケット名
	UPROPERTY(EditAnywhere)
	FName SocketName = NAME_None;

	// 攻撃ヒット設定
	UPROPERTY(EditAnywhere)
	FCommonAttackHitSettings HitSettings;

	// trueのとき、ノックバック方向をInstigator位置ではなくOwnerの前方向に固定する
	UPROPERTY(EditAnywhere)
	bool bUseOwnerForwardForKnockback = false;

	// trueのとき、NotifyBegin時点のワールドトランスフォームに当たり判定を固定する
	// ルートモーションで前進しながらボックスを動かさない場合などに使用する
	UPROPERTY(EditAnywhere)
	bool bFixWorldPosition = false;

private:

	TMap<USkeletalMeshComponent*, TSet<TWeakObjectPtr<AActor>>> ComponentHitMap;
	TMap<USkeletalMeshComponent*, FVector> PrevSocketLocationMap;
	TMap<USkeletalMeshComponent*, FTransform> FixedWorldTransformMap;

};
