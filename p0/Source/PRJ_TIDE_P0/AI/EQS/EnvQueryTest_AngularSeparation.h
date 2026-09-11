// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_AngularSeparation.generated.h"

/**
 * ターゲット(PC)から見た方位角が他の敵と近すぎる点を減点するEQSテスト
 * 他の敵とPCを結ぶ直線上に並んでシルエットが重なるのを防ぐ
 *
 * 他の敵の位置はUAIDirector::GetLaneAnchorsから取る (移動中の敵は移動先の方位で判定する)
 * 半径方向の間隔はEnvQueryTest_SlotOccupancyが見るので、こちらは角度方向のみを担当する
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_AngularSeparation : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_AngularSeparation();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 確保したい方位角の間隔 ※これ以上離れれば満点、0度で最低点
	UPROPERTY(EditAnywhere, Category = "Test", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MinSeparationDeg = 45.0f;

	// 判定基準コンテキスト (通常はEnvQueryContext_TargetActorを指定)
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

	// 他の敵の方位と候補点の判定結果を描画
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug", ClampMin = "0.0"))
	float DebugDuration = 0.15f;

};
