// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryContext_AttackTarget.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEnvQueryContext_AttackTarget::ProvideContext(FEnvQueryInstance& QueryInstance,
	FEnvQueryContextData& ContextData) const
{
	// QuerierはAIController/ポーンのどちらでも解ける
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	// AEnemyCharacter::GetTargetActorがThreatを優先して引く
	// (BBはBT運用時のミラー)
	AActor* Target = Enemy->GetTargetActor();
	if (!Target) return;

	UEnvQueryItemType_Actor::SetContextHelper(ContextData, Target);
}
