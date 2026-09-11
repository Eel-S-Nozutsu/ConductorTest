// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Suspended.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"

void UEnemyAIState_Suspended::Enter(UEnemyBrainComponent& Brain)
{
	// カットシーン中は完全に固める。移動指示が残ると滑り続け、
	// 注視が残るとターゲットへ回頭し続ける
	Brain.StopMovement();

	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}
