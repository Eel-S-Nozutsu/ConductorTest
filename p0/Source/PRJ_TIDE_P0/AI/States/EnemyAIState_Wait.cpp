// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Wait.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"

void UEnemyAIState_Wait::Enter(UEnemyBrainComponent& Brain)
{
	Elapsed = 0.0f;

	// 前の状態の移動指示が残っていると待機中に滑り続ける
	Brain.StopMovement();

	// ターゲットがいるなら向き続ける (Hold用)。いなければ解除する
	if (AActor* Target = Brain.GetTargetActor())
	{
		if (AEnemyAIController* AIController = Brain.GetAIController())
		{
			AIController->SetFocus(Target);
		}
	}
	else if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void UEnemyAIState_Wait::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	Elapsed += DeltaSeconds;
}

FString UEnemyAIState_Wait::GetDebugText() const
{
	return FString::Printf(TEXT("%.1fs"), Elapsed);
}
