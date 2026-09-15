// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "StateTree/ConductorStateTreeTasks.h"

#include "ContentConductor.h"
#include "ConductorLog.h"

#include "StateTreeExecutionContext.h"

// --- フェーズを適用 ---

FConductorTask_ApplyPhase::FConductorTask_ApplyPhase()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FConductorTask_ApplyPhase::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);

	if (UContentConductor* Conductor = Cast<UContentConductor>(Context.GetOwner()))
	{
		Conductor->EnterPhase(Data.Phase);
	}

	return EStateTreeRunStatus::Running;
}

void FConductorTask_ApplyPhase::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);

	if (UContentConductor* Conductor = Cast<UContentConductor>(Context.GetOwner()))
	{
		Conductor->ExitPhase(Data.Phase);
	}
}

// --- ログ出力 ---

FConductorTask_Log::FConductorTask_Log()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FConductorTask_Log::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);

	const UContentConductor* Conductor = Cast<UContentConductor>(Context.GetOwner());
	const FName ContentId			   = Conductor ? Conductor->GetContentId() : NAME_None;

	UE_LOG(LogConductor, Log, TEXT("[Conductor] %s: %s"), *ContentId.ToString(), *Data.Message);

	return EStateTreeRunStatus::Running;
}
