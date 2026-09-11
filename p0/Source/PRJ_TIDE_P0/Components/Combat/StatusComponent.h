// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatusComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, NewHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRevive);

/**
 * ステータス設定
 */
USTRUCT(BlueprintType)
struct FStatusSettings
{
	GENERATED_BODY()

	// 最大HP
	UPROPERTY(EditAnywhere)
	float MaxHP = 100.0f;

	// 基礎攻撃力
	UPROPERTY(EditAnywhere)
	float BaseAttackPower = 1.0f;

};

/**
 * ステータス
 */
UCLASS()
class PRJ_TIDE_P0_API UStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;

	void InitializeFromData(const FStatusSettings& Settings);
	void ModifyHP(float Delta);

	float GetCurrentHP() const { return CurrentHP; }
	float GetMaxHP() const { return MaxHP; }
	float GetBaseAttackPower() const { return BaseAttackPower; }
	bool IsDead() const { return bIsDead; }

	void ResetStatus();

	void Revive(float RestoreRatio = 1.0f);

	UPROPERTY(BlueprintAssignable)
	FOnHPChanged OnHPChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable)
	FOnRevive OnRevive;

private:

	float MaxHP = 100.0f;
	float BaseAttackPower = 10.0f;

	float CurrentHP = 0.0f;
	bool bIsDead = false;

};
