// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Wait.h"
#include "EnemyAIState_BossIdle.generated.h"

class ABossCharacter;
class UEnemyDataAsset;

/**
 * ボスの待機。BTService_BossPlayStep / BTService_BossTurnの置き換え。
 *
 * BTではBT_Bossのルートに置いたサービスだったので全ブランチで回っていたが、
 * どちらも「モンタージュ再生中はスキップ」で自分を抑止していたため、
 * 実質は待機中にしか発火していない。ここ (Idle / Hold) に置くのが実態に合う。
 *
 * ザコのUEnemyAIState_Waitを太らせないよう派生にしてある。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_BossIdle : public UEnemyAIState_Wait
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;

private:

	// 振り向き。閾値を超えた角度が続いたら回頭モンタージュを再生する
	void TickTurn(UEnemyBrainComponent& Brain, float DeltaSeconds);

	// 後退ステップ。発火方式はDAのBossStepTriggerが決める
	void TickStep(UEnemyBrainComponent& Brain, float DeltaSeconds);

	// 方向を選んでステップモンタージュを再生する。再生できたらtrue
	bool TryPlayStep(ABossCharacter* Boss, const UEnemyDataAsset* Data, AActor* Target) const;

	void ResetStepInterval(const UEnemyDataAsset* Data);

	// 閾値超えが続いた時間で減っていく回頭クールダウン
	float TurnCooldownRemaining = 0.0f;

	float TimeUntilNextStep = 0.0f;

};
