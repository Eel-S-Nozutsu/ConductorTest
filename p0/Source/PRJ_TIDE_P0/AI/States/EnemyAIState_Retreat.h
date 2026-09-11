// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Retreat.generated.h"

/**
 * 近すぎるターゲットから離れる。BTTask_ComputeRetreatLocation + MoveTo相当。
 *
 * 狙うのはKeepDistanceではなく解除ライン (GetKeepReleaseDistance)。ここへ届けば
 * IsTooCloseが解除され、優先度の再評価が別の状態へ移す。判定と後退先で同じ値を使うのが要点。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Retreat : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

private:

	// 後退先を計算して移動を開始する
	bool StartRetreatMove(UEnemyBrainComponent& Brain);

	// ターゲットがこれ以上動いたら後退先を引き直す
	static constexpr float RepathDistance = 100.0f;

	FVector LastTargetLocation = FVector::ZeroVector;

};
