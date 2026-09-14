// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ContentConductorRow.generated.h"

class UContentConductor;
class UContentConductorModule;
class UContentConductorPhaseSet;

/**
 * コンテンツの定義 行名をContentIdとする
 */
USTRUCT(BlueprintType)
struct FContentConductorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ定義"))
	TSoftObjectPtr<UContentConductorPhaseSet> PhaseSet;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するアクター"))
	TSoftObjectPtr<UDataTable> ActorTable;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するモジュール"))
	TArray<TSubclassOf<UContentConductorModule>> Modules;
};
