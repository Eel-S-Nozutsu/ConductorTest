// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Environment/WindZoneExclusion.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

AWindZoneExclusion::AWindZoneExclusion()
{
	// デバッグ描画のためだけに Tick する（判定は WindZone からの ContainsPoint 呼び出し）
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	// 可視化用シェイプ（見た目専用・コリジョン無し）。エディタでは常時ワイヤー表示される（Realtime 不要）。
	// Shape に応じて片方だけ表示する（OnConstruction で切替）。ゲーム中は既定で非表示（実行時は DrawDebug 側）。
	BoxVis = CreateDefaultSubobject<UBoxComponent>( TEXT( "BoxVis" ) );
	BoxVis->SetupAttachment( RootComponent );
	BoxVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	BoxVis->SetGenerateOverlapEvents( false );
	BoxVis->ShapeColor = FColor::Red;
	BoxVis->SetBoxExtent( BoxExtent );

	SphereVis = CreateDefaultSubobject<USphereComponent>( TEXT( "SphereVis" ) );
	SphereVis->SetupAttachment( RootComponent );
	SphereVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	SphereVis->SetGenerateOverlapEvents( false );
	SphereVis->ShapeColor = FColor::Red;
	SphereVis->SetSphereRadius( SphereRadius );
}

void AWindZoneExclusion::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	// プロパティを可視化シェイプへ反映し、Shape に応じて片方だけ表示する
	if ( BoxVis )
	{
		BoxVis->SetBoxExtent( BoxExtent );
		BoxVis->SetVisibility( Shape == EHazardAreaShape::Box );
	}
	if ( SphereVis )
	{
		SphereVis->SetSphereRadius( SphereRadius );
		SphereVis->SetVisibility( Shape == EHazardAreaShape::Sphere );
	}
}

bool AWindZoneExclusion::ContainsPoint( const FVector& WorldPoint ) const
{
	return HazardAreaUtils::ContainsPoint( Shape, GetActorTransform(), BoxExtent, SphereRadius, WorldPoint );
}

void AWindZoneExclusion::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

#if !UE_BUILD_SHIPPING
	if ( bDebugDraw )
	{
		HazardAreaUtils::DrawDebugArea( GetWorld(), Shape, GetActorTransform(), BoxExtent, SphereRadius, FColor::Red );
	}
#endif
}
