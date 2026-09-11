// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_PreferredRange.generated.h"

/**
 * 敵のPreferredRangeに近い地点を高スコアにするEQSテスト
 *
 * Score = max(0, 1 - |Dist - PreferredRange| / PreferredRange)
 * PreferredRangeちょうどで1.0、0または2xPreferredRangeで0.0
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_PreferredRange : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_PreferredRange();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// スコア計算の基準にするコンテキスト
	// ※通常はEnvQueryContext_TargetActorを指定
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

};
