// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ContentConductorPhaseRow.generated.h"

class UConductorCondition;
class UConductorPhaseAction;

/**
 * 遷移情報(条件と遷移先)
 */
USTRUCT(BlueprintType)
struct FConductorPhaseTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "条件"))
	TSubclassOf<UConductorCondition> Condition;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "遷移先のフェーズ"))
	FName NextPhase;
};

/**
 * フェーズ1つの定義
 */
USTRUCT(BlueprintType)
struct FContentConductorPhaseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ開始時のアクション"))
	TArray<TSubclassOf<UConductorPhaseAction>> EntryActions;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "遷移情報 ※上から評価"))
	TArray<FConductorPhaseTransition> Transitions;
};
