// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_PreferredMoveDistance.generated.h"

/**
 * 敵自身の現在地からの距離がPreferredMoveDistanceに近い点を高スコアにするEQSテスト
 * Score = max(0, 1 - |Dist - Preferred| / Preferred)
 * 同じ場所に留まることも、遠すぎる移動も低スコアになる
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_PreferredMoveDistance : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_PreferredMoveDistance();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

};
