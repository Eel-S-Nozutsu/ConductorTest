// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HazardArea.generated.h"

/**
 * エリアハザードの領域形状。Field・除外アクターで共通に使う。
 * Box    = アクターのローカル AABB（回転考慮）
 * Sphere = 水平半径（円柱）。判定は XY 距離のみで Z は無視する
 */
UENUM( BlueprintType )
enum class EHazardAreaShape : uint8
{
	Box		UMETA( DisplayName = "Box" ),
	Sphere	UMETA( DisplayName = "Sphere（水平半径・円柱）" ),
};

/**
 * 領域形状の共通ユーティリティ（点包含・一様乱数・デバッグ描画）。
 * Field と除外アクターが同じ仕組みで領域を扱うために共有する。
 */
namespace HazardAreaUtils
{
	// WorldPoint が領域内か。Box はローカル AABB（XYZ）、Sphere は中心からの水平距離（XY）で判定
	bool ContainsPoint(
		EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius, const FVector& WorldPoint );

	// 領域内の一様乱数点を返す（水平面。Z はアクター原点の高さ）。
	// Box はローカル ±Extent をワールドへ変換、Sphere は √r で一様化した円盤サンプリング
	FVector RandomPointInArea(
		EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius );

#if !UE_BUILD_SHIPPING
	// 領域をデバッグ描画する（Box=DrawDebugBox / Sphere=DrawDebugCylinder）
	void DrawDebugArea(
		const UWorld* World, EHazardAreaShape Shape, const FTransform& AreaTransform,
		const FVector& BoxExtent, float SphereRadius, const FColor& Color );
#endif
}
