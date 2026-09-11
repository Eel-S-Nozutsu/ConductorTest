// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Environment/WindSource.h"

#include "Components/SphereComponent.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

AWindSource::AWindSource()
{
	// デバッグ描画のためだけに Tick する（実際の風向き計算は AWindZone 側が位置を参照するだけ）
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	// 可視化用シェイプ（見た目専用・コリジョン無し）。エディタで常時ワイヤー表示される（Realtime 不要）。
	SphereVis = CreateDefaultSubobject<USphereComponent>( TEXT( "SphereVis" ) );
	SphereVis->SetupAttachment( RootComponent );
	SphereVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	SphereVis->SetGenerateOverlapEvents( false );
	SphereVis->ShapeColor = FColor::Magenta;
	SphereVis->SetSphereRadius( VisualRadius );
}

void AWindSource::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	if ( SphereVis )
	{
		SphereVis->SetSphereRadius( VisualRadius );
	}
}

void AWindSource::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

#if !UE_BUILD_SHIPPING
	if ( bDebugDraw )
	{
		UWorld* World = GetWorld();
		if ( !World ) return;

		const FVector Center = GetActorLocation();
		DrawDebugSphere( World, Center, VisualRadius, 12, FColor::Magenta, false, -1.0f );

		// 放射状に矢印を描画し、この点から全方向へ風が出ることを示す
		for ( int32 Index = 0; Index < DebugArrowCount; ++Index )
		{
			const float Angle = ( 2.0f * PI * Index ) / FMath::Max( 1, DebugArrowCount );
			const FVector Dir( FMath::Cos( Angle ), FMath::Sin( Angle ), 0.0f );
			DrawDebugDirectionalArrow( World, Center + Dir * VisualRadius, Center + Dir * ( VisualRadius + DebugArrowLength ),
				DebugArrowLength * 0.3f, FColor::Magenta, false, -1.0f, 0, 2.0f );
		}
	}
#endif
}
