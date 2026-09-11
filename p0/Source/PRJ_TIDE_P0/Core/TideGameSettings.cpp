// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

UTideGameSettings* UTideGameSettings::Get()
{
	return CastChecked<UTideGameSettings>(GEngine->GetGameUserSettings());
}
