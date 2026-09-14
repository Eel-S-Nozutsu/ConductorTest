// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorModule.h"

#include "ContentConductor.h"
#include "MapConductor.h"

void UConductorModuleBase::StartModule()
{
	OnStart();
}

void UConductorModuleBase::StopModule()
{
	OnStop();
}

void UConductorModuleBase::TickModule(float DeltaSeconds)
{
	OnTick(DeltaSeconds);
}

void UConductorModuleBase::OnStart_Implementation()
{
}

void UConductorModuleBase::OnStop_Implementation()
{
}

void UConductorModuleBase::OnTick_Implementation(float DeltaSeconds)
{
}

UMapConductor* UMapConductorModule::GetMapConductor() const
{
	return Cast<UMapConductor>(GetOuter());
}

void UContentConductorModule::EnterPhase(FName Phase)
{
	OnEnterPhase(Phase);
}

void UContentConductorModule::ExitPhase(FName Phase)
{
	OnExitPhase(Phase);
}

UContentConductor* UContentConductorModule::GetContentConductor() const
{
	return Cast<UContentConductor>(GetOuter());
}

void UContentConductorModule::OnEnterPhase_Implementation(FName Phase)
{
}

void UContentConductorModule::OnExitPhase_Implementation(FName Phase)
{
}
