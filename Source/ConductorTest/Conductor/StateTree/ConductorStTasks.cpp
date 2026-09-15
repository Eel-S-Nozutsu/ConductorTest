// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorStTasks.h"

#include "ContentConductor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "StateTreeExecutionContext.h"

FConductorStTask_OpenLevel::FConductorStTask_OpenLevel()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FConductorStTask_OpenLevel::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Level.IsNull()) return EStateTreeRunStatus::Running;

	UGameplayStatics::OpenLevelBySoftObjectPtr(Context.GetWorld(), Data.Level);

	return EStateTreeRunStatus::Running;
}
