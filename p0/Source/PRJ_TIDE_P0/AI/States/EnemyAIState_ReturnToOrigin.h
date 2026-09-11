// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_ReturnToOrigin.generated.h"

/**
 * テリトリー外に出た敵の原点復帰。BTTask_ReturnToPatrolOrigin相当。
 *
 * 帰還中は戦闘から降りている扱いで、完全無敵にしてHPゲージをグレーアウトする。
 * この解除はExitで必ず行う (BTではOnTaskFinishedが成功/失敗/中断すべてを拾っていた)。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_ReturnToOrigin : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

private:

	// 帰還先候補を求める。bRandomならテリトリー円内から一様サンプル、でなければ原点。
	// OutGoalには射影前の候補を必ず入れる(失敗描画用)。ナビ射影に成功したらtrueで
	// OutGoalを射影後の点へ更新する
	bool ComputeReturnGoal(UEnemyBrainComponent& Brain, bool bRandom, FVector& OutGoal) const;

	// 原点へ向けて移動要求を出し、成否をデバッグ線で可視化する
	// (Enter=中心狙い / 再試行=ランダム候補)
	void AttemptReturnMove(UEnemyBrainComponent& Brain, bool bRandom);

	// パスが繋がらず詰まった個体の最終保険。原点(ナビ射影)へ強制テレポートする
	void WarpHome(UEnemyBrainComponent& Brain);

	static constexpr float AcceptanceRadius = 100.0f;

	// 再試行の周期と、これだけ進捗が無ければワープする回数
	static constexpr float RetryInterval = 0.5f;
	static constexpr int32 MaxRetries    = 6;

	// 1周期でこれだけ原点へ近づけば「進捗あり」とみなす(idle/壁ずり両方を検知)
	static constexpr float ProgressEpsilon = 50.0f;

	// ランダム候補を取る半径の割合。境界ギリギリだと再判定で往復するため内側に寄せる
	static constexpr float SampleRadiusRatio = 0.8f;

	float RetryAccum       = 0.0f;
	int32 StuckStrikes     = 0;
	float LastDistToOrigin = -1.0f;

};
