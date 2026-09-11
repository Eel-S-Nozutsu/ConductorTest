// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "IDamageable.generated.h"

UINTERFACE(MinimalAPI)
class UDamageable : public UInterface
{
	GENERATED_BODY()

};

class IDamageable
{
	GENERATED_BODY()

public:

	virtual EDamageResult ReceiveDamage(const FDamageInfo& DamageInfo) = 0;
	virtual bool CanBeDamaged() const = 0;

};
