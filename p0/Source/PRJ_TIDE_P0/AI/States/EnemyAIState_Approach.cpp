// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Approach.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

void UEnemyAIState_Approach::Enter(UEnemyBrainComponent& Brain)
{
	Brain.ApplyMoveSpeed(EEnemySpeedType::Run);

	AActor* Target = Brain.GetTargetActor();
	if (!Target) return;

	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->SetFocus(Target);
	}

	LastRequestedGoal = Target->GetActorLocation();
	Brain.RequestMoveToActor(Target);
}

void UEnemyAIState_Approach::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	AActor* Target = Brain.GetTargetActor();
	if (!Target) return;

	// MoveToActorはゴール追従するが、経路自体は再計算されない
	// ターゲットが大きく動いたときだけ引き直す (毎ティック引くとパス生成が重い)
	const FVector TargetLocation = Target->GetActorLocation();
	if (FVector::DistSquared(TargetLocation, LastRequestedGoal) > FMath::Square(RepathDistance)
		|| !Brain.IsMoveInProgress())
	{
		LastRequestedGoal = TargetLocation;
		Brain.RequestMoveToActor(Target);
	}
}

void UEnemyAIState_Approach::Exit(UEnemyBrainComponent& Brain)
{
	Brain.StopMovement();
}
