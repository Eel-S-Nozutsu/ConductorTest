// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ConductorStateTreeTasks.generated.h"

/**
 * フェーズ適用タスクのインスタンスデータ
 */
USTRUCT()
struct FConductorTask_ApplyPhaseInstanceData
{
	GENERATED_BODY()

	// アクター表(DT_ActorList)のフェーズ名と一致させる
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (DisplayName = "フェーズ名"))
	FName Phase;
};

/**
 * このステートに居る間、アクター表のそのフェーズの状態を当てる
 * Conductor側のアクター管理とStateTreeのステートを繋ぐ唯一の橋
 */
USTRUCT(meta = (DisplayName = "フェーズを適用", Category = "Conductor"))
struct CONDUCTOR_API FConductorTask_ApplyPhase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FConductorTask_ApplyPhaseInstanceData;

	FConductorTask_ApplyPhase();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * ログ出力タスクのインスタンスデータ
 */
USTRUCT()
struct FConductorTask_LogInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (DisplayName = "出力するログ"))
	FString Message;
};

/**
 * ステート開始時にログを出す
 */
USTRUCT(meta = (DisplayName = "ログ出力", Category = "Conductor"))
struct CONDUCTOR_API FConductorTask_Log : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FConductorTask_LogInstanceData;

	FConductorTask_Log();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
