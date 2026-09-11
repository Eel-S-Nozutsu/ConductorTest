// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HaloSystemComponent.generated.h"

class UPrimitiveComponent;

// 光輪ユニット1つの状態。「いま光輪が何をしているか」で無防備/防御を一意に決める
// 無防備(本体ヒット＝肉質大)＝Away/Broken/Reforming
// 防御＝Deployed/Guarding
UENUM(BlueprintType)
enum class EHaloUnitState : uint8
{
	Deployed,	// 展開中: 部位に静止し受動的に守る(光輪ヒット＝ダメージ小)
	Guarding,	// ガード中: 能動防御(盾・軽減技)。無防備ではない
	Away,		// 離脱中: 光輪技で飛ばしている。無防備。技クールで戻る
	Broken,		// 破壊済み: 無防備。破壊クールで再生
	Reforming,	// 再生中: フェードインで復帰(まだ無防備)
};

// 光輪ユニットの設定。ザコ=1ユニット / ボス=部位ごとにNユニット
USTRUCT(BlueprintType)
struct FHaloUnitConfig
{
	GENERATED_BODY()

	// この光輪が守る部位の識別タグ
	UPROPERTY(EditAnywhere, Category = "HaloUnit")
	FName PartTag;

	// 展開中(光輪ヒット)のダメージ倍率。1.0=従来どおり、下げると光輪で硬くなる
	UPROPERTY(EditAnywhere, Category = "HaloUnit", meta = (ClampMin = "0.0"))
	float HaloHitDamageMultiplier = 1.0f;

	// 無防備(本体ヒット)のダメージ倍率。大きいほど痛い
	UPROPERTY(EditAnywhere, Category = "HaloUnit", meta = (ClampMin = "0.0"))
	float BodyHitDamageMultiplier = 1.5f;

	// 破壊後、再生するまでのクールタイム
	UPROPERTY(EditAnywhere, Category = "HaloUnit", meta = (ClampMin = "0.0"))
	float BreakRegenCooldown = 8.0f;

};

/**
 * 全敵共通の光輪システム。光輪ユニット(ザコ=1 / ボス=部位ごとN)の状態を1箇所で管理し、
 * 無防備・肉質・再生クールなどの共通挙動を全ユニットに適用する。
 * 既存のHalo/BackHalo/Partを段階的にここへ寄せていく。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class PRJ_TIDE_P0_API UHaloSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHaloSystemComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ユニット設定を流し込む。空ならno-op(この敵は共通光輪を使わない)
	void Initialize(const TArray<FHaloUnitConfig>& InUnits);

	// item4: 部位の被弾ダメージ倍率。展開中＝光輪ヒット(小)/ 無防備＝本体ヒット(大)
	// 該当ユニットが無ければ1.0(倍率なし)
	float GetPartDamageMultiplier(FName PartTag) const;

	// その部位が無防備(Away/Broken/Reforming)か
	bool IsPartDefenseless(FName PartTag) const;

	// その部位に光輪ユニットが登録されているか
	bool HasUnit(FName PartTag) const;

	// ヒットコンポーネントのタグから対応ユニットのPartTagを返す(無ければNone)
	// PartDestructionと同じくコンポーネントタグ＝PartTagで照合する
	FName GetUnitPartTagForComponent(const UPrimitiveComponent* HitComp) const;

	// 状態遷移(移行段階で各挙動から呼ぶ)
	void SetUnitState(FName PartTag, EHaloUnitState NewState);
	EHaloUnitState GetUnitState(FName PartTag) const;

private:

	// 設定＋実行時状態。ユニットはInitializeで確定し以降
	// 増減しないのでPartTag引きのTMapで持つ
	struct FHaloUnit
	{
		FHaloUnitConfig Config;
		EHaloUnitState State = EHaloUnitState::Deployed;
		float RegenTimer = 0.0f;	// Broken中の残り再生時間

	};

	// 再生を自前タイマーで回すか。ザコ=true
	// /ボスは破壊・復活をPartDestructionがミラー駆動するためfalse
	bool bAutoRegen = false;

	TMap<FName, FHaloUnit> Units;

};
