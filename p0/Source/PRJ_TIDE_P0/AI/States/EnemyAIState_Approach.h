// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Approach.generated.h"

/**
 * ターゲットへ接近する。BT_CombatのIsTooFarブランチ相当。
 *
 * ターゲットは動くので目的地の再指定が要る。距離が閾値を割れば
 * IsTooFarがfalseになり、優先度の再評価が勝手に別の状態へ移す。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Approach : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

private:

	// ターゲットがこれ以上動いたら経路を引き直す
	static constexpr float RepathDistance = 100.0f;

	FVector LastRequestedGoal = FVector::ZeroVector;

};
