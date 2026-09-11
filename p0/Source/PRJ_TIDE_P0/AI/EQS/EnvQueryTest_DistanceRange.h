// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_DistanceRange.generated.h"

/**
 * DataAssetのMinEngageRange〜MaxEngageRangeで距離フィルターをかけるEQSテスト
 * AIDirectorによる難易度調整時にMinEngageRange/MaxEngageRangeを変更することで
 * 敵の行動圏をランタイムで制御できる
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_DistanceRange : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_DistanceRange();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// スコア計算の基準にするコンテキスト
	// (通常はEnvQueryContext_TargetActorを指定)
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

};
