// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/HazardArea.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

namespace HazardAreaUtils
{
	bool ContainsPoint(
		EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius, const FVector& WorldPoint )
	{
		switch ( Shape )
		{
		case EHazardAreaShape::Box:
		{
			const FVector Local = AreaTransform.InverseTransformPosition( WorldPoint );
			return FMath::Abs( Local.X ) <= BoxExtent.X
				&& FMath::Abs( Local.Y ) <= BoxExtent.Y
				&& FMath::Abs( Local.Z ) <= BoxExtent.Z;
		}
		case EHazardAreaShape::Sphere:
		{
			// 水平半径（円柱）判定。Z は無視する
			const FVector Center = AreaTransform.GetLocation();
			return FVector::DistSquared2D( WorldPoint, Center ) <= FMath::Square( SphereRadius );
		}
		default:
			return false;
		}
	}

	FVector RandomPointInArea(
		EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius )
	{
		switch ( Shape )
		{
		case EHazardAreaShape::Box:
		{
			// ローカル ±Extent の水平面をワールドへ変換（回転考慮）
			const FVector Local(
				FMath::FRandRange( -BoxExtent.X, BoxExtent.X ),
				FMath::FRandRange( -BoxExtent.Y, BoxExtent.Y ),
				0.0f );
			return AreaTransform.TransformPosition( Local );
		}
		case EHazardAreaShape::Sphere:
		{
			// √r で一様化した円盤サンプリング（水平面）。中心はアクター原点
			const float Angle = FMath::FRandRange( 0.0f, 2.0f * PI );
			const float R = SphereRadius * FMath::Sqrt( FMath::FRand() );
			const FVector Center = AreaTransform.GetLocation();
			return Center + FVector( R * FMath::Cos( Angle ), R * FMath::Sin( Angle ), 0.0f );
		}
		default:
			return AreaTransform.GetLocation();
		}
	}

#if !UE_BUILD_SHIPPING
	void DrawDebugArea(
		const UWorld* World, EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius, const FColor& Color )
	{
		if ( !World ) return;

		switch ( Shape )
		{
		case EHazardAreaShape::Box:
			DrawDebugBox( World, AreaTransform.GetLocation(), BoxExtent, AreaTransform.GetRotation(),
				Color, false, -1.0f, 0, 2.0f );
			break;
		case EHazardAreaShape::Sphere:
		{
			// 判定は XY のみだが、見た目は薄い円柱で描く
			const FVector Center = AreaTransform.GetLocation();
			constexpr float DrawHalfHeight = 100.0f;
			DrawDebugCylinder( World, Center - FVector( 0, 0, DrawHalfHeight ), Center + FVector( 0, 0, DrawHalfHeight ),
				SphereRadius, 24, Color, false, -1.0f, 0, 2.0f );
			break;
		}
		default:
			break;
		}
	}
#endif
}
