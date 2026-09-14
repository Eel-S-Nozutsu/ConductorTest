// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ContentConductorPhaseSet.generated.h"

class UConductorCondition;
class UConductorPhaseAction;

/**
 * 遷移情報(条件と遷移先)
 */
USTRUCT(BlueprintType)
struct FConductorPhaseTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, meta = (DisplayName = "条件"))
	TObjectPtr<UConductorCondition> Condition;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "遷移先のフェーズ"))
	FName NextPhase;
};

/**
 * フェーズ1つの定義
 */
USTRUCT(BlueprintType)
struct FContentConductorPhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, meta = (DisplayName = "フェーズ開始時のアクション"))
	TArray<TObjectPtr<UConductorPhaseAction>> EntryActions;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "遷移情報"))
	TArray<FConductorPhaseTransition> Transitions;
};

/**
 * 1コンテンツ分のフェーズ定義一式
 */
UCLASS(BlueprintType)
class CONDUCTOR_API UContentConductorPhaseSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, meta = (DisplayName = "コンテンツ開始条件 (空なら即開始)"))
	TObjectPtr<UConductorCondition> StartCondition;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "開始時に入るフェーズ"))
	FName InitialPhase;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "フェーズ定義", ForceInlineRow))
	TMap<FName, FContentConductorPhase> Phases;
};
