// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_AllyProximity.generated.h"

/**
 * 他の敵の現在位置に近い候補点を減点するEQSテスト
 * 集団戦で移動先が味方の足元に集中して団子になるのを防ぐ
 *
 * 他の敵の位置はUAIDirector::GetCombatantLocationsから取る (移動先ではなく実位置)。
 * DesiredPosition基準のSlotOccupancyと違い点密度に依らず効くので、密集時の団子に強い。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_AllyProximity : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_AllyProximity();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 減点する半径 ※これ以上離れれば満点、真上で最低点
	UPROPERTY(EditAnywhere, Category = "Test", meta = (ClampMin = "0.0"))
	float ProximityRadius = 200.0f;

	// 真上に付与する最低スコア ※0で全否定、上げると密集時の全滅を防ぐ
	UPROPERTY(EditAnywhere, Category = "Test", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinScore = 0.0f;

	// 他の敵の位置と候補点の判定結果を描画
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug", ClampMin = "0.0"))
	float DebugDuration = 0.15f;

};
