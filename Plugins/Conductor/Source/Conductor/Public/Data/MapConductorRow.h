// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MapConductorRow.generated.h"

class UMapConductor;
class UMapConductorModule;

/**
 * レベルとMapConductorの紐づけ
 */
USTRUCT(BlueprintType)
struct FMapConductorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "対象のレベル"))
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このレベルのコンテンツ一覧"))
	TSoftObjectPtr<UDataTable> ContentConductorList;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このレベルが管理するモジュール"))
	TArray<TSubclassOf<UMapConductorModule>> Modules;
};
