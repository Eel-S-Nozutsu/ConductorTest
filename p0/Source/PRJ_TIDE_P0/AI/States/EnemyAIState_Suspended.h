// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Suspended.generated.h"

/**
 * カットシーン中の一時停止。PCのシネマモード中に最上位優先で入る。
 *
 * 移動と注視追従を止めてその場で固まるだけ。抜けるのはシネマモード解除後の
 * 優先度再評価に任せる (Reactと同じく、条件が消えれば次の状態へ抜ける)。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Suspended : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;

};
