// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "ConductorTestGameSettings.generated.h"

/**
 * ユーザー設定
 */
UCLASS()
class UConductorTestGameSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TMap<FString, bool> ImGuiWindowOpened;

	static UConductorTestGameSettings* Get();
};
