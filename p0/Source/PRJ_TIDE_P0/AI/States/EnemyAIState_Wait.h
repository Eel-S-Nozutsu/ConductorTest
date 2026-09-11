// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Wait.generated.h"

/**
 * その場待機。BTのWaitノード相当。
 *
 * Idle (ターゲットなし) とHold (交戦距離内・ボス) の両方に同じクラスを使う。
 * 待つ以外にやることが無く、抜けるかどうかは優先度の再評価が決めるため、
 * ここは経過時間を数えるだけでよい (時間はデバッグ表示と、後でボスのステップ再生に使う)。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Wait : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;

	virtual FString GetDebugText() const override;

private:

	float Elapsed = 0.0f;

};
