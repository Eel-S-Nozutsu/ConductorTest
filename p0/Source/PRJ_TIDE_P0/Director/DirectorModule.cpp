// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "DirectorModule.h"

#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/Director/MapDirector.h"

void UDirectorModuleBase::StartModule()
{
	OnStart();
}

void UDirectorModuleBase::StopModule()
{
	OnStop();
}

void UDirectorModuleBase::TickModule(float DeltaSeconds)
{
	OnTick(DeltaSeconds);
}

void UDirectorModuleBase::OnStart_Implementation()
{
}

void UDirectorModuleBase::OnStop_Implementation()
{
}

void UDirectorModuleBase::OnTick_Implementation(float DeltaSeconds)
{
}

UMapDirector* UMapDirectorModule::GetMapDirector() const
{
	return Cast<UMapDirector>(GetOuter());
}

void UContentDirectorModule::EnterPhase(FName Phase)
{
	OnEnterPhase(Phase);
}

void UContentDirectorModule::ExitPhase(FName Phase)
{
	OnExitPhase(Phase);
}

UContentDirector* UContentDirectorModule::GetContentDirector() const
{
	return Cast<UContentDirector>(GetOuter());
}

void UContentDirectorModule::OnEnterPhase_Implementation(FName Phase)
{
}

void UContentDirectorModule::OnExitPhase_Implementation(FName Phase)
{
}
