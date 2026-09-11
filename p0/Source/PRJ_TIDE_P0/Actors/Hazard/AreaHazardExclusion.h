// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRJ_TIDE_P0/Actors/Hazard/HazardArea.h"
#include "AreaHazardExclusion.generated.h"

class UBoxComponent;
class USphereComponent;

/**
 * エリアハザードの除外領域。落雷（Strike）の抽選対象から外したい範囲に自由配置する。
 * AAreaHazardField が自エリア内の除外アクターを収集し、抽選点がこの領域内なら棄却する。
 * 形状・判定は HazardAreaUtils を共有（Box / Sphere＝水平半径・円柱）。
 * 汎用フレーム [Docs/AreaHazard.md] の一部。
 */
UCLASS()
class PRJ_TIDE_P0_API AAreaHazardExclusion : public AActor
{
	GENERATED_BODY()

public:
	AAreaHazardExclusion();

	// WorldPoint がこの除外領域内か
	bool ContainsPoint( const FVector& WorldPoint ) const;

protected:
	// 実行中（PIE/ゲーム）の DrawDebug 用。エディタでの常時表示は可視化シェイプ（BoxVis/SphereVis）が担う
	virtual void Tick( float DeltaTime ) override;
	// 形状プロパティを可視化コンポーネント（Box/Sphere）へ同期する（配置・プロパティ変更時に走る）
	virtual void OnConstruction( const FTransform& Transform ) override;

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardExclusion" )
	TObjectPtr<USceneComponent> SceneRoot;

	// エディタで常時ワイヤー表示するための可視化用シェイプ（Realtime 不要）。Shape に応じて片方だけ表示する。
	// 判定はあくまで HazardAreaUtils::ContainsPoint で行い、これらは見た目専用（コリジョン無し）
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardExclusion" )
	TObjectPtr<UBoxComponent> BoxVis;

	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardExclusion" )
	TObjectPtr<USphereComponent> SphereVis;

	// 領域形状
	UPROPERTY( EditAnywhere, Category = "Tide|HazardExclusion" )
	EHazardAreaShape Shape = EHazardAreaShape::Box;

	// Box のローカル半径（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardExclusion", meta = ( EditCondition = "Shape == EHazardAreaShape::Box", EditConditionHides ) )
	FVector BoxExtent = FVector( 300.0f, 300.0f, 300.0f );

	// Sphere（水平半径・円柱）の半径（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardExclusion", meta = ( EditCondition = "Shape == EHazardAreaShape::Sphere", EditConditionHides ) )
	float SphereRadius = 300.0f;

	// ON で領域を赤枠で毎フレーム描画する（除外だと分かるように）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardExclusion|Debug" )
	bool bDebugDraw = true;
};
