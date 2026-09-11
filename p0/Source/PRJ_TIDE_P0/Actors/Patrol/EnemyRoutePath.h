// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyRoutePath.generated.h"

class USplineComponent;

/**
 * 敵の待機ルート。レベル上に配置したスプラインをEnemyのRouteMove待機が参照する。
 */
UCLASS()
class PRJ_TIDE_P0_API AEnemyRoutePath : public AActor
{
	GENERATED_BODY()

public:

	AEnemyRoutePath();

	int32 GetRoutePointCount() const;
	int32 FindNearestRoutePointIndex(const FVector& WorldLocation) const;
	FVector GetLocationAtRoutePointIndex(int32 PointIndex) const;

	float GetRouteLength() const;
	float FindNearestDistanceAlongRoute(const FVector& WorldLocation) const;
	FVector GetLocationAtDistanceAlongRoute(float Distance) const;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Tide|AI|Route")
	TObjectPtr<USplineComponent> Spline = nullptr;

};
