// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorPhaseAction.h"

#include "Conductor/ContentConductor.h"

void UConductorPhaseAction::Execute_Implementation(UContentConductor* Conductor)
{
}

void UConductorPhaseAction_Log::Execute_Implementation(UContentConductor* Conductor)
{
	const FName ContentId = Conductor ? Conductor->GetContentId() : NAME_None;

	UE_LOG(LogTemp, Log, TEXT("[Conductor] %s: %s"), *ContentId.ToString(), *Message);
}
