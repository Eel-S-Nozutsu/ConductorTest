// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_NotCrossingAlly.generated.h"

/**
 * そこへ移動する途中で他の敵の前を横切る点を減点するEQSテスト
 *
 * ストレイフはPCを中心に回る動きなので、PCを原点とした方位角で考える。
 * 自分の現在方位から候補点の方位へ回る円弧 (短い方) の中に他の敵の方位が入っていれば、
 * その敵とPCの間を通過する = 横切ることになる。
 *
 * 到達不能にはしないようScoreで効かせる (Filterにすると狭い場所で候補が全滅しうる)。
 * 他の敵の位置はEnvQueryTest_AngularSeparationと同じくUAIDirector::GetLaneAnchorsから取る。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_NotCrossingAlly : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_NotCrossingAlly();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 横切ると判定された点のスコア ※0で最低、0.3程度で「他に選択肢がなければ許す」
	UPROPERTY(EditAnywhere, Category = "Test", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrossingScore = 0.0f;

	// 判定基準コンテキスト ※通常はEnvQueryContext_TargetActorを指定
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

	// trueで横切り判定を可視化
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug", ClampMin = "0.0"))
	float DebugDuration = 0.15f;

};
