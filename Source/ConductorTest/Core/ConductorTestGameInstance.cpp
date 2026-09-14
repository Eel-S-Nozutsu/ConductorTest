// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "Core/ConductorTestGameInstance.h"
#include "Core/ConductorTestGameSettings.h"

void UConductorTestGameInstance::Init()
{
	Super::Init();
}

void UConductorTestGameInstance::Shutdown()
{
	if (UConductorTestGameSettings* Settings = UConductorTestGameSettings::Get())
	{
		Settings->SaveSettings();
	}

	Super::Shutdown();
}
