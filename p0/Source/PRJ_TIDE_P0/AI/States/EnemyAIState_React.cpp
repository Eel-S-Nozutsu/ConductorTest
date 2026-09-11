// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_React.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

void UEnemyAIState_React::Enter(UEnemyBrainComponent& Brain)
{
	OwningBrain = &Brain;

	// 移動と注視を落とすだけ。以降はリアクションが終わってIsReacting()がfalseになれば
	// 優先度の再評価が勝手に次の状態へ移す
	Brain.StopMovement();

	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

FString UEnemyAIState_React::GetDebugText() const
{
	const AEnemyCharacter* Enemy = OwningBrain ? OwningBrain->GetEnemy() : nullptr;
	if (!Enemy) return FString();

	const FName Reason = Enemy->GetReactionReason();

	// 理由なしでReactにいる = デバッグのAI停止
	// (UEnemyBrainComponent::IsReactingが拾う)
	return Reason.IsNone() ? TEXT("DebugAIStop") : Reason.ToString();
}
