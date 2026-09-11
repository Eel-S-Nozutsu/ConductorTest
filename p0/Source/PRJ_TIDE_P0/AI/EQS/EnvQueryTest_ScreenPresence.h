// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_ScreenPresence.generated.h"

/**
 * スクリーン外の位置を減点するEQSテスト
 * オンスクリーン: 1.0 / オフスクリーン: 0.0
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_ScreenPresence : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_ScreenPresence();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

};
