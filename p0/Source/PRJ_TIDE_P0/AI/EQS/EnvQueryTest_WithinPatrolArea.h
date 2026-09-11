// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_WithinPatrolArea.generated.h"

/**
 * BBのPatrolOrigin/PatrolRadiusで定義されたパトロール範囲外の候補点を除外するEQSテスト。
 * PatrolRadiusが0の場合(スポナー未使用など)は全候補を通過させる。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_WithinPatrolArea : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_WithinPatrolArea();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

};
