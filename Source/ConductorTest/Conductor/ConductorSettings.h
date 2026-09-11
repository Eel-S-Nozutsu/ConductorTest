// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "ConductorSettings.generated.h"

/**
 * コンダクタが参照するDTと共通設定
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "コンダクター設定"))
class CONDUCTORTEST_API UConductorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UConductorSettings();

	static const UConductorSettings* Get() { return GetDefault<UConductorSettings>(); }

	// マップコンダクタの設定
	UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "レベル紐づけ"))
	TSoftObjectPtr<UDataTable> MapConductorTable;

	// 遷移条件評価の間隔
	UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "条件評価の間隔(秒)", ClampMin = "0.0"))
	float ConditionEvaluateInterval = 0.1f;
};
