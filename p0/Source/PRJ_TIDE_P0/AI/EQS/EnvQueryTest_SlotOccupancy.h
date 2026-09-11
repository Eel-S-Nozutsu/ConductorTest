// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_SlotOccupancy.generated.h"

/**
 * 他の敵の移動先(DesiredPosition)に近い点を減点するEQSテスト
 * OccupancyRadius以内に他の敵が登録した位置があれば距離に応じてスコアを下げる
 *
 * 現在位置基準のAllyProximityと対で、こちらは「同時に近くへ移動する」団子を防ぐ
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_SlotOccupancy : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_SlotOccupancy();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 減点半径。移動先からこれ以上離れれば満点 ※0以下でAIDirectorの既定を使用
	UPROPERTY(EditAnywhere, Category = "Test", meta = (ClampMin = "0.0"))
	float OccupancyRadius = 150.0f;

	// 他の敵の移動先と候補点の判定結果を描画
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug", ClampMin = "0.0"))
	float DebugDuration = 0.15f;

};
