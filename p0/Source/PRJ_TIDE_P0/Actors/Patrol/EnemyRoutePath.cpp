// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Patrol/EnemyRoutePath.h"

#include "Components/SplineComponent.h"

AEnemyRoutePath::AEnemyRoutePath()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;

	if (Spline)
	{
		Spline->SetClosedLoop(false);
	}
}

int32 AEnemyRoutePath::GetRoutePointCount() const
{
	return Spline ? Spline->GetNumberOfSplinePoints() : 0;
}

int32 AEnemyRoutePath::FindNearestRoutePointIndex(const FVector& WorldLocation) const
{
	if (!Spline) return INDEX_NONE;

	const int32 PointCount = Spline->GetNumberOfSplinePoints();
	if (PointCount <= 0) return INDEX_NONE;

	const float ClosestInputKey = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return FMath::Clamp(FMath::RoundToInt(ClosestInputKey), 0, PointCount - 1);
}

FVector AEnemyRoutePath::GetLocationAtRoutePointIndex(int32 PointIndex) const
{
	if (!Spline) return GetActorLocation();

	const int32 PointCount = Spline->GetNumberOfSplinePoints();
	if (PointCount <= 0) return Spline->GetComponentLocation();

	const int32 ClampedPointIndex = FMath::Clamp(PointIndex, 0, PointCount - 1);
	return Spline->GetLocationAtSplinePoint(ClampedPointIndex, ESplineCoordinateSpace::World);
}

float AEnemyRoutePath::GetRouteLength() const
{
	return Spline ? Spline->GetSplineLength() : 0.0f;
}

float AEnemyRoutePath::FindNearestDistanceAlongRoute(const FVector& WorldLocation) const
{
	if (!Spline) return 0.0f;

	const float InputKey = Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return Spline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

FVector AEnemyRoutePath::GetLocationAtDistanceAlongRoute(float Distance) const
{
	if (!Spline) return GetActorLocation();

	const float RouteLength = Spline->GetSplineLength();
	if (RouteLength <= KINDA_SMALL_NUMBER)
	{
		return Spline->GetComponentLocation();
	}

	const float ClampedDistance = FMath::Clamp(Distance, 0.0f, RouteLength);
	return Spline->GetLocationAtDistanceAlongSpline(ClampedDistance, ESplineCoordinateSpace::World);
}
