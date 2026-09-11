// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_LineOfSight.generated.h"

/**
 * 移動先+EyeHeightOffsetからPCへ視線が通るかをチェックするEQSテスト
 * 視線が通る: 1.0 / 遮られる: 0.0
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_LineOfSight : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_LineOfSight();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 視線の飛ばし先コンテキスト ※通常はEnvQueryContext_TargetActorを指定
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

};
