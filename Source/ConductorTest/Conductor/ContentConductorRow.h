// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ContentConductorRow.generated.h"

class UContentConductor;
class UContentConductorModule;

/**
 * コンテンツの定義 行名をContentIdとする
 */
USTRUCT(BlueprintType)
struct FContentConductorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ定義"))
	TSoftObjectPtr<UDataTable> PhaseTable;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "生成直後に入るフェーズ(PhaseTableの行名)"))
	FName InitialPhase;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するアクター"))
	TSoftObjectPtr<UDataTable> ActorTable;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "このコンテンツが制御するモジュール"))
	TArray<TSubclassOf<UContentConductorModule>> Modules;
};
