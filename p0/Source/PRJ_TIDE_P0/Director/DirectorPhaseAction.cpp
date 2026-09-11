// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "DirectorPhaseAction.h"

#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/PRJ_TIDE_P0.h"
#include "PRJ_TIDE_P0/Subsystems/Signal/TideSignalSubsystem.h"

void UDirectorPhaseAction::Execute_Implementation(UContentDirector* Director)
{
}

void UDirectorPhaseAction_BroadcastSignal::Execute_Implementation(UContentDirector* Director)
{
	if (SignalName.IsNone()) return;

	if (UTideSignalSubsystem* Signals = GetWorld() ? GetWorld()->GetSubsystem<UTideSignalSubsystem>() : nullptr)
	{
		Signals->BroadcastSignal(SignalName);
	}
}

void UDirectorPhaseAction_Log::Execute_Implementation(UContentDirector* Director)
{
	const FName ContentId = Director ? Director->GetContentId() : NAME_None;

	UE_LOG(LogPRJ_TIDE_P0, Log, TEXT("[Director] %s: %s"), *ContentId.ToString(), *Message);
}
