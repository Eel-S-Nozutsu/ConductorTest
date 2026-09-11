// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_NotBehindTarget.generated.h"

/**
 * ターゲット(PC)の向こう側にある点を除外するEQSテスト
 * 敵→ターゲット方向との内積がDotThreshold以上 かつ ターゲット以遠の点をフィルターする
 * 敵がPCを通り抜けなければ到達できない位置への移動を防ぐ
 */
UCLASS()
class PRJ_TIDE_P0_API UEnvQueryTest_NotBehindTarget : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_NotBehindTarget();

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	// 除外コーンの広さ ※cos値0.7≈45度、1.0に近いほど後方のみ除外
	UPROPERTY(EditAnywhere, Category = "Filter")
	float DotThreshold = 0.7f;

	// ターゲット距離の余裕 ※PCのカプセル半径+バッファ程度に設定する
	UPROPERTY(EditAnywhere, Category = "Filter")
	float MarginRadius = 80.0f;

	// 判定基準コンテキスト ※通常はEnvQueryContext_TargetActorを指定
	UPROPERTY(EditAnywhere, Category = "Context")
	TSubclassOf<UEnvQueryContext> TargetContext;

	// trueで判定結果を可視化
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition = "bDrawDebug", ClampMin = "0.0"))
	float DebugDuration = 0.15f;

};
