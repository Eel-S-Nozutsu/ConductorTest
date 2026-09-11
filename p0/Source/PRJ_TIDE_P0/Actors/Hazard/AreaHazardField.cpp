// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardField.h"

#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"	// TActorIterator
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardStrike.h"
#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardExclusion.h"
#include "PRJ_TIDE_P0/Actors/Spawner/TideSpawner.h"
#include "PRJ_TIDE_P0/Actors/Prop/BreakableProp.h"
#include "PRJ_TIDE_P0/Actors/Environment/SurfaceRideZone.h"
#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

AAreaHazardField::AAreaHazardField()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	// 作動範囲のエディタ可視化（見た目専用）。半径・可視性は OnConstruction で反映する
	ActivationCullVis = CreateDefaultSubobject<USphereComponent>( TEXT( "ActivationCullVis" ) );
	ActivationCullVis->SetupAttachment( SceneRoot );
	ActivationCullVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	ActivationCullVis->SetGenerateOverlapEvents( false );
	ActivationCullVis->ShapeColor = FColor::Orange;
	ActivationCullVis->SetVisibility( false );
	ActivationCullVis->SetHiddenInGame( true );
}

void AAreaHazardField::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	if ( ActivationCullVis )
	{
		// 描画半径にはアクタースケール（最小成分）が乗るので、割り戻してワールド実寸に合わせる
		const FVector Scale = GetActorScale3D();
		const float ShapeScale = FMath::Max( KINDA_SMALL_NUMBER,
			FMath::Min3( FMath::Abs( Scale.X ), FMath::Abs( Scale.Y ), FMath::Abs( Scale.Z ) ) );
		ActivationCullVis->SetSphereRadius( FMath::Max( 0.0f, ActivationCullRadius ) / ShapeScale );
		ActivationCullVis->SetVisibility( bDrawActivationCullRange );
	}
}

void AAreaHazardField::BeginPlay()
{
	Super::BeginPlay();

	CollectExclusions();

	if ( bAutoStart )
	{
		StartHazard();
	}
}

void AAreaHazardField::StartHazard()
{
	bRunning = true;
	// 開始直後に一斉に出ないよう、1周期ぶん待たせる
	SpawnTimer = FMath::Max( 0.01f, SpawnInterval + FMath::FRandRange( -IntervalJitter, IntervalJitter ) );
	PlayerSpawnTimer = FMath::Max( 0.01f, PlayerSpawnInterval + FMath::FRandRange( -PlayerIntervalJitter, PlayerIntervalJitter ) );
}

void AAreaHazardField::StopHazard()
{
	bRunning = false;
}

void AAreaHazardField::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// プレイヤーが範囲外なら Strike のスポーン処理を止める
	const bool bWithinCull = TideProximityGate::IsPlayerWithinDistance( this, GetActorLocation(), ActivationCullRadius, CachedPlayerPawn );

#if !UE_BUILD_SHIPPING
	if ( bDrawActivationCullRange )
	{
		TideProximityGate::DrawActivationRange( this, GetActorLocation(), ActivationCullRadius, bWithinCull );
	}

	if ( bDebugDraw )
	{
		HazardAreaUtils::DrawDebugArea( GetWorld(), Shape, GetActorTransform(), BoxExtent, SphereRadius, FColor::Green );
	}
#endif

	if ( !bRunning ) return;

	if ( !bWithinCull ) return;

	SpawnTimer -= DeltaTime;
	if ( SpawnTimer <= 0.0f )
	{
		SpawnStrike();
		SpawnTimer = FMath::Max( 0.01f, SpawnInterval + FMath::FRandRange( -IntervalJitter, IntervalJitter ) );
	}

	// プレイヤー中心落としはランダム抽選と並行して動く
	if ( bEnablePlayerCenteredSpawn )
	{
		PlayerSpawnTimer -= DeltaTime;
		if ( PlayerSpawnTimer <= 0.0f )
		{
			SpawnPlayerCenteredStrike();
			PlayerSpawnTimer = FMath::Max( 0.01f, PlayerSpawnInterval + FMath::FRandRange( -PlayerIntervalJitter, PlayerIntervalJitter ) );
		}
	}
}

void AAreaHazardField::CollectExclusions()
{
	Exclusions.Reset();

	UWorld* World = GetWorld();
	if ( !World ) return;

	// 自エリア内に中心がある除外アクターを自動収集する
	for ( TActorIterator<AAreaHazardExclusion> It( World ); It; ++It )
	{
		AAreaHazardExclusion* Excl = *It;
		if ( !Excl ) continue;
		if ( HazardAreaUtils::ContainsPoint( Shape, GetActorTransform(), BoxExtent, SphereRadius, Excl->GetActorLocation() ) )
		{
			Exclusions.AddUnique( Excl );
		}
	}

	// 手動指定。エリア外に中心があり縁で重なる除外など
	for ( const TObjectPtr<AAreaHazardExclusion>& Excl : ManualExclusions )
	{
		if ( Excl )
		{
			Exclusions.AddUnique( Excl );
		}
	}
}

bool AAreaHazardField::ResolveGroundPoint( const FVector& CandidateXY, float NewRadius, FVector& OutLocation, FVector& OutNormal ) const
{
	const UWorld* World = GetWorld();
	if ( !World ) return false;

	// 地面トレースは Pawn を拾わないよう ObjectType（WorldStatic＋WorldDynamic）で行う
	FCollisionObjectQueryParams GroundQuery;
	GroundQuery.AddObjectTypesToQuery( ECC_WorldStatic );
	GroundQuery.AddObjectTypesToQuery( ECC_WorldDynamic );
	FCollisionQueryParams TraceParams( SCENE_QUERY_STAT( HazardFieldGround ), false, this );

	const FVector Start = CandidateXY + FVector( 0, 0, TraceHeight );
	const FVector End   = Start - FVector( 0, 0, TraceHeight + TraceDistance );

	TArray<FHitResult> Hits;
	if ( !World->LineTraceMultiByObjectType( Hits, Start, End, GroundQuery, TraceParams ) )
	{
		return false;
	}

	// Spawner・壊れ物・面沿いゾーンは飛ばし、その奥（下）の本当の地面まで貫通させる
	const FHitResult* GroundHit = nullptr;
	for ( const FHitResult& Hit : Hits )
	{
		const AActor* HitActor = Hit.GetActor();
		if ( HitActor && ( HitActor->IsA<ATideSpawner>() || HitActor->IsA<ABreakableProp>() || HitActor->IsA<ASurfaceRideZone>() ) )
		{
			continue;
		}
		GroundHit = &Hit;
		break;
	}
	if ( !GroundHit )
	{
		return false;
	}
	const FVector Ground = GroundHit->ImpactPoint;

#if !UE_BUILD_SHIPPING
	if ( bDebugDraw && GEngine )
	{
		const AActor* HitActor = GroundHit->GetActor();
		const UPrimitiveComponent* HitComponent = GroundHit->GetComponent();
		GEngine->AddOnScreenDebugMessage( -1, 3.0f, FColor::Yellow, FString::Printf(
			TEXT( "[AreaHazardField] Ground hit: Actor=%s Component=%s" ),
			HitActor ? *HitActor->GetName() : TEXT( "None" ),
			HitComponent ? *HitComponent->GetName() : TEXT( "None" ) ) );
	}
#endif

	for ( const TWeakObjectPtr<AAreaHazardExclusion>& Weak : Exclusions )
	{
		const AAreaHazardExclusion* Excl = Weak.Get();
		if ( Excl && Excl->ContainsPoint( Ground ) )
		{
			return false;
		}
	}

	// 進行中 Strike の範囲と重なる地点は棄却する
	if ( bPreventStrikeOverlap && OverlapsActiveStrike( Ground, NewRadius ) )
	{
		return false;
	}

	OutLocation = Ground;
	OutNormal = GroundHit->ImpactNormal;
	return true;
}

bool AAreaHazardField::OverlapsActiveStrike( const FVector& Location, float NewRadius ) const
{
	// 範囲は world Z 方向の縦筒なので、footprint の重なり＝水平距離で判定する
	for ( const TWeakObjectPtr<AAreaHazardStrike>& Weak : ActiveStrikes )
	{
		const AAreaHazardStrike* Strike = Weak.Get();
		if ( !Strike ) continue;

		const float MinDist = Strike->StrikeRadius + NewRadius + StrikeOverlapMargin;
		if ( FVector::DistSquared2D( Location, Strike->GetActorLocation() ) < MinDist * MinDist )
		{
			return true;
		}
	}
	return false;
}

float AAreaHazardField::GetStrikeRadiusFromClass( TSubclassOf<AAreaHazardStrike> InStrikeClass )
{
	if ( const AAreaHazardStrike* CDO = InStrikeClass.GetDefaultObject() )
	{
		return CDO->StrikeRadius;
	}
	return 0.0f;
}

bool AAreaHazardField::SampleStrikeLocation( float NewRadius, FVector& OutLocation, FVector& OutNormal ) const
{
	const FTransform AreaTransform = GetActorTransform();

	for ( int32 Attempt = 0; Attempt < MaxSampleAttempts; ++Attempt )
	{
		const FVector Candidate = HazardAreaUtils::RandomPointInArea( Shape, AreaTransform, BoxExtent, SphereRadius );
		if ( ResolveGroundPoint( Candidate, NewRadius, OutLocation, OutNormal ) )
		{
			return true;
		}
	}

	return false;
}

bool AAreaHazardField::SamplePlayerCenteredLocation( const FVector& PlayerLocation, float NewRadius, FVector& OutLocation, FVector& OutNormal ) const
{
	for ( int32 Attempt = 0; Attempt < MaxSampleAttempts; ++Attempt )
	{
		// √r で一様化した円盤サンプリング（HazardAreaUtils の Sphere 分岐と同じ手法）
		const float Angle = FMath::FRandRange( 0.0f, 2.0f * PI );
		const float R = PlayerAreaRadius * FMath::Sqrt( FMath::FRand() );
		const FVector Candidate = PlayerLocation + FVector( R * FMath::Cos( Angle ), R * FMath::Sin( Angle ), 0.0f );

		if ( !ResolveGroundPoint( Candidate, NewRadius, OutLocation, OutNormal ) )
		{
			continue;
		}

		// 見えない予兆を出さないため、映っていない地点は棄却して再抽選する
		if ( bPlayerSpawnRequireOnScreen && !IsPointOnPlayerScreen( OutLocation ) )
		{
			continue;
		}

		return true;
	}

	return false;
}

bool AAreaHazardField::IsPointOnPlayerScreen( const FVector& WorldPoint ) const
{
	// どちらかでも 0 以下ならフィルタ無効＝常に画面内扱い
	if ( PlayerSpawnScreenFractionX <= 0.0f || PlayerSpawnScreenFractionY <= 0.0f ) return true;

	APlayerController* PC = UGameplayStatics::GetPlayerController( this, 0 );
	if ( !PC ) return true;	// PC が取れない場合はフィルタしない

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize( SizeX, SizeY );
	if ( SizeX <= 0 || SizeY <= 0 ) return true;

	// カメラ背後・画面外への投影は false が返る
	FVector2D ScreenPos;
	if ( !PC->ProjectWorldLocationToScreen( WorldPoint, ScreenPos, false ) )
	{
		return false;
	}

	// 中央基準で画面サイズの Frac 割の矩形内か（神技ロックオンの画面内判定と同方式）
	const float MarginX = SizeX * ( 1.0f - PlayerSpawnScreenFractionX ) * 0.5f;
	const float MarginY = SizeY * ( 1.0f - PlayerSpawnScreenFractionY ) * 0.5f;

	return ScreenPos.X >= MarginX && ScreenPos.X <= ( SizeX - MarginX )
		&& ScreenPos.Y >= MarginY && ScreenPos.Y <= ( SizeY - MarginY );
}

TSubclassOf<AAreaHazardStrike> AAreaHazardField::PickStrikeClass( const TArray<FAreaHazardStrikeOption>& Options ) const
{
	float TotalWeight = 0.0f;	// 有効候補＝クラスあり・重み正
	for ( const FAreaHazardStrikeOption& Option : Options )
	{
		if ( Option.StrikeClass && Option.Weight > 0.0f )
		{
			TotalWeight += Option.Weight;
		}
	}
	if ( TotalWeight <= 0.0f )
	{
		return nullptr;
	}

	float Pick = FMath::FRandRange( 0.0f, TotalWeight );
	for ( const FAreaHazardStrikeOption& Option : Options )
	{
		if ( !Option.StrikeClass || Option.Weight <= 0.0f ) continue;
		Pick -= Option.Weight;
		if ( Pick <= 0.0f )
		{
			return Option.StrikeClass;
		}
	}
	// 浮動小数の端で抜けた場合の保険
	for ( int32 i = Options.Num() - 1; i >= 0; --i )
	{
		if ( Options[i].StrikeClass && Options[i].Weight > 0.0f )
		{
			return Options[i].StrikeClass;
		}
	}
	return nullptr;
}

bool AAreaHazardField::IsPlayerInArea( const APawn* PlayerPawn ) const
{
	if ( !PlayerPawn ) return false;
	return HazardAreaUtils::ContainsPoint( Shape, GetActorTransform(), BoxExtent, SphereRadius, PlayerPawn->GetActorLocation() );
}

void AAreaHazardField::SpawnStrike()
{
	if ( MaxConcurrentStrikes > 0 && PruneAndCountActiveStrikes() >= MaxConcurrentStrikes )
	{
		return;
	}

	// クラスを先に決め、その StrikeRadius をかぶり防止の抽選に渡す
	const TSubclassOf<AAreaHazardStrike> Picked = PickStrikeClass( StrikeOptions );
	if ( !Picked ) return;

	FVector Location, Normal;
	if ( !SampleStrikeLocation( GetStrikeRadiusFromClass( Picked ), Location, Normal ) )
	{
		return;	// 除外だらけ・地面無し・かぶり回避
	}

	SpawnStrikeAt( Location, Normal, Picked );
}

void AAreaHazardField::SpawnPlayerCenteredStrike()
{
	// 同時進行上限はランダム抽選と共有する
	if ( MaxConcurrentStrikes > 0 && PruneAndCountActiveStrikes() >= MaxConcurrentStrikes )
	{
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn( this, 0 );
	if ( !IsPlayerInArea( PlayerPawn ) )
	{
		return;	// 次のタイマー周期で再判定する
	}

	// 専用候補から選ぶ。未設定ならランダム抽選と同じ StrikeOptions にフォールバック
	TSubclassOf<AAreaHazardStrike> Picked = PickStrikeClass( PlayerStrikeOptions );
	if ( !Picked )
	{
		Picked = PickStrikeClass( StrikeOptions );
	}
	if ( !Picked ) return;

	FVector Location, Normal;
	if ( !SamplePlayerCenteredLocation( PlayerPawn->GetActorLocation(), GetStrikeRadiusFromClass( Picked ), Location, Normal ) )
	{
		return;	// 除外だらけ・地面無し・画面外・かぶり回避
	}

	SpawnStrikeAt( Location, Normal, Picked );
}

void AAreaHazardField::SpawnStrikeAt( const FVector& Location, const FVector& Normal, TSubclassOf<AAreaHazardStrike> InStrikeClass )
{
	if ( !InStrikeClass ) return;

	UWorld* World = GetWorld();
	if ( !World ) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;	// 敵味方判定のソース（このアクター＝NoTeam）
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAreaHazardStrike* Strike = World->SpawnActor<AAreaHazardStrike>(
		InStrikeClass, Location, FRotator::ZeroRotator, SpawnParams );
	if ( Strike )
	{
		Strike->Activate( Location, Normal );
		ActiveStrikes.Add( Strike );
	}
}

int32 AAreaHazardField::PruneAndCountActiveStrikes()
{
	for ( int32 i = ActiveStrikes.Num() - 1; i >= 0; --i )
	{
		if ( !ActiveStrikes[i].IsValid() )
		{
			ActiveStrikes.RemoveAt( i );
		}
	}
	return ActiveStrikes.Num();
}
