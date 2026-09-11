// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnemyAIState_Strafe.generated.h"

/**
 * 交戦距離内での回り込み。BT_Combatの
 * RunEQS → RegisterMoveTarget → MoveTo → Wait 1sブランチ相当。
 *
 * EQSは非同期なので、クエリ中・移動中・待機中の3相を自前で持つ。
 * BTを廃止してもEQSテスト11本の資産はそのまま使える(クエリを直接実行するだけ)。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Strafe : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

	virtual FString GetDebugText() const override;

private:

	enum class EPhase : uint8
	{
		Querying,
		Moving,
		Waiting,
	};

	void StartQuery(UEnemyBrainComponent& Brain);
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	// 移動先決定後の待機 (BTのWait 1s)
	static constexpr float WaitDuration = 1.0f;

	// クエリ失敗時の再試行間隔 ※毎ティック投げないようにする
	static constexpr float RetryInterval = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyBrainComponent> OwningBrain = nullptr;

	EPhase Phase = EPhase::Waiting;

	float WaitRemaining = 0.0f;

	// Exit後に届いたクエリ結果を捨てるための世代番号
	// EQSは非同期なので、状態を抜けた後に結果が返ってくることがある
	int32 QueryGeneration = 0;

};
