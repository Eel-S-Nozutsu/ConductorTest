// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Interfaces/Debug/IImGuiInspectable.h"
#include "TideCharacter.generated.h"

/**
 * キャラクター基底
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API ATideCharacter
	: public ACharacter
	, public IDamageable
	, public IImGuiInspectable
{
	GENERATED_BODY()

public:

	ATideCharacter();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// このアクターに属する全プリミティブ (本体メッシュ・ビジュアル用子メッシュ・装備品・
	// HaloComponent等) の「デカール受け取り」を一括設定する。地面へ投影する視野デバッグ扇や
	// 着弾予告デカールが体・装備に滲むのを防ぐ用途。ランタイムで装備を付け替えたら再度呼ぶ
	void SetReceivesDecalsOnAllMeshes(bool bReceives);

	// IDamageable
	virtual EDamageResult ReceiveDamage(const FDamageInfo& DamageInfo) override;
	virtual bool CanBeDamaged() const override;

	// IImGuiInspectable: HP等の共通情報を表示
	// サブクラスでoverrideして追加可能
	virtual void DrawImGuiInspector() override;

	// 攻撃ヒット時にFDamageInfoを自身で修飾・上書きするためのフック。Targetはヒット対象
	virtual void OnModifyDamageInfo(FDamageInfo& DamageInfo, FGameplayTag AttackTypeTag, AActor* Target = nullptr) {}

	// ダメージが相手に確定したとき1回だけ呼ばれる
	virtual void OnAttackHitConfirmed(const FDamageInfo& DamageInfo) {}

	void AddStateTag(const FGameplayTag& Tag);
	void RemoveStateTag(const FGameplayTag& Tag);
	bool HasStateTag(const FGameplayTag& Tag) const;
	bool HasAnyStateTag(const FGameplayTagContainer& GameplayTags) const;

	const class UTideCharacterDataAsset* GetCharacterData() const { return CharacterData; }
	void SetCharacterData(class UTideCharacterDataAsset* InData) { CharacterData = InData; }

	class UStatusComponent* GetStatusComponent() const { return StatusComponent; }

	class UHazardDotComponent* GetHazardDotComponent() const { return HazardDotComponent; }

protected:

	UPROPERTY(EditAnywhere, Category = "Tide|Data")
	TObjectPtr<class UTideCharacterDataAsset> CharacterData = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UStatusComponent> StatusComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UDamageSystemComponent> DamageSystem = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UHitReactionComponent> HitReactionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UStateTagComponent> StateTagComponent = nullptr;

	// 継続ダメージ(しびれ床など)の受け側。エリアハザードのZoneからRefreshDotされる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UHazardDotComponent> HazardDotComponent = nullptr;

private:

	// DrawImGuiInspectorの各CollapsingHeaderセクション
	void DrawImGuiStatusSection();
	void DrawImGuiStateTagsSection();
	void DrawImGuiAnimationSection();

};
