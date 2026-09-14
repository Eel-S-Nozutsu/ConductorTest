// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "Core/ConductorTestGameSettings.h"

UConductorTestGameSettings* UConductorTestGameSettings::Get()
{
	return CastChecked<UConductorTestGameSettings>(GEngine->GetGameUserSettings());
}
