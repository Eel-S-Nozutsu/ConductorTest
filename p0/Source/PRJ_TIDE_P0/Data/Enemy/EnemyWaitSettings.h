// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "EnemyWaitSettings.generated.h"

class AEnemyRoutePath;

UENUM(BlueprintType)
enum class EEnemyWaitType : uint8
{
	RandomPatrol UMETA(DisplayName = "ランダム巡回"),
	RouteMove    UMETA(DisplayName = "ルート移動待機"),
};

UENUM(BlueprintType)
enum class EEnemyRouteMoveType : uint8
{
	ForwardLoop UMETA(DisplayName = "順ループ型"),
	PingPongLoop UMETA(DisplayName = "往復ループ型"),
	OneWayStop UMETA(DisplayName = "終着型"),
};

USTRUCT(BlueprintType)
struct FEnemyWaitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait")
	EEnemyWaitType WaitType = EEnemyWaitType::RandomPatrol;

	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait", meta = (EditCondition = "WaitType == EEnemyWaitType::RouteMove", EditConditionHides))
	TObjectPtr<AEnemyRoutePath> RoutePath = nullptr;

	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait", meta = (EditCondition = "WaitType == EEnemyWaitType::RouteMove", EditConditionHides))
	EEnemyRouteMoveType RouteMoveType = EEnemyRouteMoveType::ForwardLoop;

	// 各ルートポイント到達後の待機秒数
	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait", meta = (ClampMin = "0.0", EditCondition = "WaitType == EEnemyWaitType::RouteMove", EditConditionHides))
	float WaitAtRoutePointTime = 0.0f;

	// 目標点にどれだけ近づけば到達扱いにするか
	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait", meta = (ClampMin = "10.0", EditCondition = "WaitType == EEnemyWaitType::RouteMove", EditConditionHides))
	float RouteAcceptanceRadius = 80.0f;

	// 終着型で終点到達後に再生する待機モンタージュ ※ループはモンタージュ側で設定
	UPROPERTY(EditAnywhere, Category = "Tide|AI|Wait", meta = (EditCondition = "WaitType == EEnemyWaitType::RouteMove && RouteMoveType == EEnemyRouteMoveType::OneWayStop", EditConditionHides))
	TObjectPtr<UAnimMontage> TerminalIdleMontage = nullptr;

};
