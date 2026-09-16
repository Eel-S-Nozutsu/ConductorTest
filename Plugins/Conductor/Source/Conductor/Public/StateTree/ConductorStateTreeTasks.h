// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ConductorStateTreeTasks.generated.h"

/**
 * FConductorTask_ApplyPhase用 インスタンスデータ
 */
USTRUCT()
struct FConductorTask_ApplyPhaseInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ名"))
	FName Phase;
};

/**
 * このステート中フェーズの状態を適用
 */
USTRUCT(meta = (DisplayName = "フェーズを適用", Category = "Conductor"))
struct CONDUCTOR_API FConductorTask_ApplyPhase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FConductorTask_ApplyPhaseInstanceData;

	FConductorTask_ApplyPhase();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};

/**
 * FConductorTask_Log用 インスタンスデータ
 */
USTRUCT()
struct FConductorTask_LogInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "出力するログ"))
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

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
