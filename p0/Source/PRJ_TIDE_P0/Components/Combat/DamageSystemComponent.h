// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "DamageSystemComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageReceived, const FDamageInfo&, DamageInfo);

/**
 * ダメージを計算してステータスに適用する
 */
UCLASS()
class PRJ_TIDE_P0_API UDamageSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	void Initialize(class UStatusComponent* InStatus);

	void ProcessDamage(const FDamageInfo& DamageInfo);

	UPROPERTY(BlueprintAssignable)
	FOnDamageReceived OnDamageReceived;

private:

	bool ValidateDamage(const FDamageInfo& DamageInfo) const;

	UPROPERTY()
	class UStatusComponent* StatusComponent = nullptr;

};
