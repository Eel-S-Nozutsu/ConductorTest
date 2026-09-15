// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeTaskBase.h"
#include "ConductorStTasks.generated.h"

USTRUCT()
struct FConductorStTask_OpenLevelInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (DisplayName = "遷移先のレベル"))
	TSoftObjectPtr<UWorld> Level;
};

/**
 * ステート開始時にレベルを開く
 */
USTRUCT(meta = (DisplayName = "レベルを開く", Category = "Conductor"))
struct FConductorStTask_OpenLevel : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FConductorStTask_OpenLevelInstanceData;

	FConductorStTask_OpenLevel();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
