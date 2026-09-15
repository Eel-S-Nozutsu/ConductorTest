// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ContentConductorRow.generated.h"

class UContentConductor;
class UContentConductorModule;
class UStateTree;

/**
 * コンテンツの定義 行名をContentIdとする
 */
USTRUCT(BlueprintType)
struct FContentConductorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ定義"))
	TSoftObjectPtr<UStateTree> StateTree;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するアクター"))
	TSoftObjectPtr<UDataTable> ActorTable;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するモジュール"))
	TArray<TSubclassOf<UContentConductorModule>> Modules;
};
