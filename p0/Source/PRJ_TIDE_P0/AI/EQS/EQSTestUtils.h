// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "AIController.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

namespace EQSTestUtils
{
inline AEnemyCharacter* ResolveQuerierEnemy(const FEnvQueryInstance& QueryInstance)
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(QueryInstance.Owner.Get()))
	{
		return Enemy;
	}

	if (AAIController* AIC = Cast<AAIController>(QueryInstance.Owner.Get()))
	{
		return Cast<AEnemyCharacter>(AIC->GetPawn());
	}

	return nullptr;
}
} // namespace EQSTestUtils
