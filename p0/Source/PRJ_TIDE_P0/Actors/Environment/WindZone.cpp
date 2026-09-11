// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Environment/WindZone.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ArrowComponent.h"
#include "Math/RandomStream.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "PRJ_TIDE_P0/Actors/Environment/WindZoneExclusion.h"
#include "PRJ_TIDE_P0/Actors/Environment/WindSource.h"
#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

AWindZone::AWindZone()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	WindArrow = CreateDefaultSubobject<UArrowComponent>( TEXT( "WindArrow" ) );
	WindArrow->SetupAttachment( RootComponent );
	WindArrow->ArrowColor = FColor::Cyan;
	WindArrow->ArrowLength = 200.0f;

	// 可視化用シェイプ（見た目専用・コリジョン無し）。エディタで常時ワイヤー表示
	BoxVis = CreateDefaultSubobject<UBoxComponent>( TEXT( "BoxVis" ) );
	BoxVis->SetupAttachment( RootComponent );
	BoxVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	BoxVis->SetGenerateOverlapEvents( false );
	BoxVis->ShapeColor = FColor::Cyan;
	BoxVis->SetBoxExtent( BoxExtent );

	SphereVis = CreateDefaultSubobject<USphereComponent>( TEXT( "SphereVis" ) );
	SphereVis->SetupAttachment( RootComponent );
	SphereVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	SphereVis->SetGenerateOverlapEvents( false );
	SphereVis->ShapeColor = FColor::Cyan;
	SphereVis->SetSphereRadius( SphereRadius );

	// 半径・可視性は OnConstruction で反映する
	ActivationCullVis = CreateDefaultSubobject<USphereComponent>( TEXT( "ActivationCullVis" ) );
	ActivationCullVis->SetupAttachment( RootComponent );
	ActivationCullVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	ActivationCullVis->SetGenerateOverlapEvents( false );
	ActivationCullVis->ShapeColor = FColor::Orange;
	ActivationCullVis->SetVisibility( false );
	ActivationCullVis->SetHiddenInGame( true );
}

void AWindZone::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	// Shape に応じて片方だけ表示する
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

	// 作動範囲＝境界（Box は対角長／Sphere は半径）＋マージンの球
	if ( ActivationCullVis )
	{
		const float BoundingRadius = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.Size() : SphereRadius;
		ActivationCullVis->SetSphereRadius( FMath::Max( 0.0f, BoundingRadius + ActivationCullMargin ) );
		ActivationCullVis->SetVisibility( bDrawActivationCullRange );
	}
}

void AWindZone::BeginPlay()
{
	Super::BeginPlay();

	GenerateDebugFlowSeeds();
	CollectExclusions();

	SpawnWindFlowVFX();
}

void AWindZone::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// 遠距離ゲート：プレイヤーがゾーンから十分離れていれば、範囲内走査＋遮蔽トレースをスキップする
	const float BoundingRadius = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.Size() : SphereRadius;
	const bool bWithinCull = TideProximityGate::IsPlayerWithinDistance( this, GetActorLocation(), BoundingRadius + ActivationCullMargin, CachedPlayerPawn );
	if ( bWithinCull )
	{
		ApplyWind( DeltaTime );
		UpdatePlayerFrontVFX( DeltaTime );
	}
	else
	{
		// MOVE_Flying 等に取り残さないよう解放する（空集合なら何もしない軽量な呼び出し）
		ReleaseAllWindTargets();
		if ( PlayerFrontVFXComponent && PlayerFrontVFXComponent->IsActive() )
		{
			PlayerFrontVFXComponent->Deactivate();
		}
	}

#if !UE_BUILD_SHIPPING
	if ( bDrawActivationCullRange )
	{
		TideProximityGate::DrawActivationRange( this, GetActorLocation(), BoundingRadius + ActivationCullMargin, bWithinCull );
	}

	if ( bDebugDraw )
	{
		HazardAreaUtils::DrawDebugArea( GetWorld(), Shape, GetActorTransform(), BoxExtent, SphereRadius, FColor::Cyan );

		// 中心から、その地点での風向きへ（FromSource 時は発生源からの方向のサンプル表示）
		const FVector Center = GetActorLocation();
		const FVector Dir = ComputeWindDirectionAt( Center );
		DrawDebugDirectionalArrow( GetWorld(), Center, Center + Dir * 300.0f, 120.0f, FColor::Yellow, false, -1.0f, 0, 4.0f );

		DrawDebugFlowArrows();
		DrawDebugLateralGradient();
	}
#endif
}

void AWindZone::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	for ( const FWindFlowVFXInstance& Inst : WindFlowVFXInstances )
	{
		if ( UNiagaraComponent* Comp = Inst.Component.Get() )
		{
			Comp->Deactivate();
		}
	}
	WindFlowVFXInstances.Reset();

	if ( PlayerFrontVFXComponent )
	{
		PlayerFrontVFXComponent->Deactivate();
	}

	ReleaseAllWindTargets();
	Super::EndPlay( EndPlayReason );
}

void AWindZone::SpawnWindFlowVFX()
{
	if ( !WindFlowVFX ) return;

	// 位置は最初にランダムで決めて一括生成し、以降はループ再生し続ける（毎フレームの生成・寿命処理は無い）
	const int32 Count = FMath::Max( 0, WindFlowVFXMaxConcurrent );
	for ( int32 i = 0; i < Count; ++i )
	{
		TrySpawnWindFlowVFX();
	}
}

void AWindZone::TrySpawnWindFlowVFX()
{
	if ( !WindFlowVFX ) return;

	// 最小間隔を満たせば即採用、リトライ上限まで見つからなければ最も離れた候補を使う。
	// 無効エリア内の候補は除外し、有効な候補が 1 つも出なければ生成しない
	const float MinSepSq = FMath::Square( WindFlowVFXMinSeparation );
	const int32 Attempts = FMath::Max( 1, WindFlowVFXMaxSampleAttempts );

	FVector BestPos = FVector::ZeroVector;
	float BestMinDistSq = -1.0f;
	bool bFound = false;

	for ( int32 i = 0; i < Attempts; ++i )
	{
		const FVector Candidate = RandomWindFlowLocalPos();

		if ( IsInExclusion( GetActorTransform().TransformPosition( Candidate ) ) )
		{
			continue;
		}

		float MinDistSq = TNumericLimits<float>::Max();
		for ( const FWindFlowVFXInstance& Inst : WindFlowVFXInstances )
		{
			MinDistSq = FMath::Min( MinDistSq, FVector::DistSquared( Candidate, Inst.LocalPos ) );
		}

		if ( WindFlowVFXInstances.Num() == 0 || MinDistSq >= MinSepSq )
		{
			BestPos = Candidate;
			bFound = true;
			break;
		}

		// まだ満たさない：最も離れた候補を保持しておく
		if ( MinDistSq > BestMinDistSq )
		{
			BestMinDistSq = MinDistSq;
			BestPos = Candidate;
			bFound = true;
		}
	}

	if ( !bFound ) return;	// 全候補が無効エリア内だった

	// Root へアタッチして相対回転ゼロにすると、アセット既定の +X フローがアクター前方＝風向きに一致する
	UNiagaraComponent* Comp = NewObject<UNiagaraComponent>( this );
	if ( !Comp ) return;

	Comp->SetupAttachment( RootComponent );
	Comp->SetRelativeLocation( BestPos );
	Comp->SetRelativeRotation( FRotator::ZeroRotator );
	Comp->SetAutoActivate( false );
	Comp->SetAsset( WindFlowVFX );
	Comp->RegisterComponent();
	Comp->Activate( true );

	FWindFlowVFXInstance Inst;
	Inst.Component = Comp;
	Inst.LocalPos = BestPos;
	WindFlowVFXInstances.Add( Inst );
}

FVector AWindZone::RandomWindFlowLocalPos() const
{
	if ( Shape == EHazardAreaShape::Box )
	{
		return FVector(
			FMath::FRandRange( -BoxExtent.X, BoxExtent.X ),
			FMath::FRandRange( -BoxExtent.Y, BoxExtent.Y ),
			FMath::FRandRange( -BoxExtent.Z, BoxExtent.Z ) );
	}
	// 球内一様サンプリング（半径方向は cbrt で一様化）
	const float R = SphereRadius * FMath::Pow( FMath::FRand(), 1.0f / 3.0f );
	return FMath::VRand() * R;
}

void AWindZone::UpdatePlayerFrontVFX( float DeltaTime )
{
	// 内容は同じなので、専用スロット未設定なら WindFlowVFX を流用する
	UNiagaraSystem* Asset = PlayerFrontVFX ? PlayerFrontVFX : WindFlowVFX;

	// ApplyWind が同フレーム先行して WindAffectedActors を更新済み
	APawn* Player = UGameplayStatics::GetPlayerPawn( this, 0 );
	const bool bAffected = bEnablePlayerFrontVFX && Asset && Player && WindAffectedActors.Contains( Player );

	if ( bAffected )
	{
		// プレイヤー位置を基準に前方＋高さオフセットした位置へ、風向きを向けて出す
		const FVector PlayerLoc = Player->GetActorLocation();
		const FVector WindDir = ComputeWindDirectionAt( PlayerLoc );
		const FVector WorldPos = PlayerLoc + WindDir * PlayerFrontVFXForwardOffset + FVector( 0, 0, PlayerFrontVFXHeightOffset );
		const FRotator WorldRot = WindDir.Rotation();	// エフェクト既定の +X フローを風向きへ合わせる

		if ( !PlayerFrontVFXComponent )
		{
			PlayerFrontVFXComponent = NewObject<UNiagaraComponent>( this );
			PlayerFrontVFXComponent->SetupAttachment( RootComponent );
			PlayerFrontVFXComponent->SetAutoActivate( false );
			PlayerFrontVFXComponent->SetAsset( Asset );
			PlayerFrontVFXComponent->RegisterComponent();
		}

		// アタッチではなくワールド指定で毎フレーム更新し、風向きを常に保つ
		PlayerFrontVFXComponent->SetWorldLocationAndRotation( WorldPos, WorldRot );

		if ( !PlayerFrontVFXComponent->IsActive() )
		{
			PlayerFrontVFXComponent->Activate( true );
		}
	}
	else if ( PlayerFrontVFXComponent && PlayerFrontVFXComponent->IsActive() )
	{
		// 影響外になったら Deactivate（ループ停止・残パーティクルはフェード）
		PlayerFrontVFXComponent->Deactivate();
	}
}

void AWindZone::ApplyWind( float DeltaTime )
{
	UWorld* World = GetWorld();
	if ( !World ) return;

	const FCollisionShape CollShape = ( Shape == EHazardAreaShape::Box )
		? FCollisionShape::MakeBox( BoxExtent )
		: FCollisionShape::MakeSphere( SphereRadius );

	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( WindZone ), false, this );

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType( Overlaps, GetActorLocation(), GetActorQuat(), ObjQuery, CollShape, Params );

	// AreaTargets＝エリア内（無効エリア除く・遮蔽無視）／Targets＝実際に風を受けている対象
	TSet<TWeakObjectPtr<AActor>> CurrentAreaTargets;
	TSet<TWeakObjectPtr<AActor>> CurrentTargets;
	for ( const FOverlapResult& Overlap : Overlaps )
	{
		AActor* Other = Overlap.GetActor();
		if ( !Other || Other == this ) continue;
		if ( CurrentAreaTargets.Contains( Other ) ) continue;

		IWindAffectable* Affectable = Cast<IWindAffectable>( Other );
		if ( !Affectable ) continue;

		// 無効エリア内へは配らない（Exit は範囲離脱と同じ経路で発火する）
		const FVector OtherLocation = Other->GetActorLocation();
		if ( IsInExclusion( OtherLocation ) ) continue;

		// ここまで来た対象は遮蔽に関係なくエリアメンバーとして扱う
		CurrentAreaTargets.Add( Other );
		if ( !WindAreaActors.Contains( Other ) )
		{
			Affectable->OnWindZoneAreaEnter();
		}

		// 押し出し方向と遮蔽トレース方向はそれぞれ独立したモードで対象位置ごとに決まる
		const FVector WindDir = ComputeWindDirectionAt( OtherLocation );
		const FVector TraceDir = ComputeOcclusionTraceDirectionAt( OtherLocation );

		// 「体の何割が露出しているか」と左右幅グラデーションを掛け合わせる。
		// 合成が実質 0 なら押し出しは配らないが、エリア判定には残る
		const float Exposure = bEnableOcclusion ? ComputeOcclusionExposure( Other, TraceDir ) : 1.0f;
		const float WindScale = Exposure * ComputeLateralGradientScale( OtherLocation );
		if ( WindScale <= KINDA_SMALL_NUMBER ) continue;

		CurrentTargets.Add( Other );

		FWindInfluence TargetWind;
		TargetWind.Source = this;
		TargetWind.HorizontalWindVelocity = WindDir * WindPushSpeed * WindScale;

		if ( !WindAffectedActors.Contains( Other ) )
		{
			Affectable->OnWindEnter( TargetWind );
		}
		Affectable->OnWindTick( TargetWind, DeltaTime );
	}

	// 押し出しから外れた対象へ Exit（範囲離脱＋遮蔽で露出 0 になった対象を含む）
	for ( const TWeakObjectPtr<AActor>& Prev : WindAffectedActors )
	{
		if ( CurrentTargets.Contains( Prev ) ) continue;
		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IWindAffectable* Affectable = Cast<IWindAffectable>( PrevActor ) )
			{
				Affectable->OnWindExit();
			}
		}
	}

	// エリアから外れた対象へ AreaExit（遮蔽では外れず、無効エリア進入／範囲離脱でのみ外れる）
	for ( const TWeakObjectPtr<AActor>& Prev : WindAreaActors )
	{
		if ( CurrentAreaTargets.Contains( Prev ) ) continue;
		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IWindAffectable* Affectable = Cast<IWindAffectable>( PrevActor ) )
			{
				Affectable->OnWindZoneAreaExit();
			}
		}
	}

	WindAffectedActors = MoveTemp( CurrentTargets );
	WindAreaActors = MoveTemp( CurrentAreaTargets );
}

void AWindZone::CollectExclusions()
{
	Exclusions.Reset();

	UWorld* World = GetWorld();
	if ( !World ) return;

	// 自エリア内に中心があるものを自動収集する
	for ( TActorIterator<AWindZoneExclusion> It( World ); It; ++It )
	{
		AWindZoneExclusion* Excl = *It;
		if ( !Excl ) continue;
		if ( HazardAreaUtils::ContainsPoint( Shape, GetActorTransform(), BoxExtent, SphereRadius, Excl->GetActorLocation() ) )
		{
			Exclusions.AddUnique( Excl );
		}
	}

	// 手動指定を追加（エリア外に中心があり縁で重なるものなど）
	for ( const TObjectPtr<AWindZoneExclusion>& Excl : ManualExclusions )
	{
		if ( Excl )
		{
			Exclusions.AddUnique( Excl );
		}
	}
}

bool AWindZone::IsInExclusion( const FVector& WorldPoint ) const
{
	for ( const TWeakObjectPtr<AWindZoneExclusion>& Weak : Exclusions )
	{
		const AWindZoneExclusion* Excl = Weak.Get();
		if ( Excl && Excl->ContainsPoint( WorldPoint ) )
		{
			return true;
		}
	}
	return false;
}

FVector AWindZone::ComputeDirectionForMode( EWindDirectionMode Mode, const FVector& WorldPoint, bool bFlattenToHorizontal ) const
{
	if ( Mode == EWindDirectionMode::FromSource && WindSourceActor )
	{
		const FVector Delta = WorldPoint - WindSourceActor->GetActorLocation();
		const FVector Dir = bFlattenToHorizontal ? Delta.GetSafeNormal2D() : Delta.GetSafeNormal();
		if ( !Dir.IsNearlyZero() )
		{
			return Dir;
		}
	}
	// Fixed、または発生源未設定／対象が発生源と同一地点ならアクター前方へフォールバック
	const FVector Forward = GetActorForwardVector();
	return bFlattenToHorizontal ? Forward.GetSafeNormal2D() : Forward.GetSafeNormal();
}

FVector AWindZone::ComputeWindDirectionAt( const FVector& WorldPoint ) const
{
	// 風は常に水平（この仕組み自体が水平方向の押し出し前提）
	return ComputeDirectionForMode( WindDirectionMode, WorldPoint, /*bFlattenToHorizontal=*/ true );
}

FVector AWindZone::ComputeOcclusionTraceDirectionAt( const FVector& WorldPoint ) const
{
	// 発生源と対象の高さが違う場合も、その角度どおりにトレースするため水平化しない
	return ComputeDirectionForMode( OcclusionDirectionMode, WorldPoint, /*bFlattenToHorizontal=*/ false );
}

float AWindZone::ComputeLateralGradientScale( const FVector& WorldPoint ) const
{
	if ( !bEnableLateralGradient || LateralGradientWidth <= KINDA_SMALL_NUMBER ) return 1.0f;

	// 左右幅＝風向きに直交するローカル Y 軸。球は前後で横断面が狭まるが、
	// 分かりやすさ優先で半径一律を基準にする（主用途は Box の通路）
	const float HalfWidth = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.Y : SphereRadius;
	if ( HalfWidth <= KINDA_SMALL_NUMBER ) return 1.0f;

	const float LateralOffset = FMath::Abs( GetActorTransform().InverseTransformPosition( WorldPoint ).Y );
	// 端からの距離（0=端／HalfWidth=中央）を減衰帯で正規化し、LateralEdgeScale → 1.0 へ線形補間
	const float DistFromEdge = HalfWidth - LateralOffset;
	const float Alpha = FMath::Clamp( DistFromEdge / LateralGradientWidth, 0.0f, 1.0f );
	return FMath::Lerp( LateralEdgeScale, 1.0f, Alpha );
}

float AWindZone::ComputeOcclusionExposure( const AActor* Target, const FVector& TraceDir ) const
{
	UWorld* World = GetWorld();
	if ( !World || !Target ) return 1.0f;

	// 対象の横幅を OcclusionSampleCount 本で均等サンプリングする
	float CollisionRadius = 0.0f, CollisionHalfHeight = 0.0f;
	Target->GetSimpleCollisionCylinder( CollisionRadius, CollisionHalfHeight );
	const FVector Center = Target->GetActorLocation();

	// TraceDir が真上/真下に近いと UpVector との外積が潰れるため、その場合だけ ForwardVector を基準にする
	const bool bTraceDirNearlyVertical = FMath::Abs( FVector::DotProduct( TraceDir, FVector::UpVector ) ) > 0.99f;
	const FVector LateralReference = bTraceDirNearlyVertical ? FVector::ForwardVector : FVector::UpVector;
	const FVector LateralAxis = FVector::CrossProduct( TraceDir, LateralReference ).GetSafeNormal();

	// 半径が取れない（コリジョン無し等）対象は中心 1 本だけ見る
	const int32 SampleCount = ( CollisionRadius > KINDA_SMALL_NUMBER ) ? FMath::Max( 1, OcclusionSampleCount ) : 1;

	// この距離以内なら完全遮蔽。超えた分は OcclusionTraceDistance に向けて露出 0→1 へ線形に戻る
	const float FalloffStartDistance = OcclusionTraceDistance * OcclusionFalloffStartRatio;
	const float FalloffRange = FMath::Max( KINDA_SMALL_NUMBER, OcclusionTraceDistance - FalloffStartDistance );

	// bTraceComplex=true でシンプルコリジョン未設定のメッシュ（ブロックアウト等）も拾う
	FCollisionQueryParams Params( SCENE_QUERY_STAT( WindZoneOcclusion ), true, this );
	Params.AddIgnoredActor( Target );

	float ExposureSum = 0.0f;
	for ( int32 Index = 0; Index < SampleCount; ++Index )
	{
		// 1 なら中心のみ、2 本以上なら左端〜右端を均等割り（両端含む）
		const float Alpha = ( SampleCount == 1 ) ? 0.5f : ( float( Index ) / float( SampleCount - 1 ) );
		const FVector SamplePoint = Center + LateralAxis * ( CollisionRadius * ( 2.0f * Alpha - 1.0f ) );

		// 必ず「対象→トレース方向の逆」の向きにする。逆向きだと起点が壁の内部に埋まり、
		// 複雑コリジョンがヒットを取りこぼすことがある
		const FVector UpwindPoint = SamplePoint - TraceDir * OcclusionTraceDistance;

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel( Hit, SamplePoint, UpwindPoint, OcclusionTraceChannel, Params );

		const float SampleExposure = bBlocked ? FMath::Clamp( ( Hit.Distance - FalloffStartDistance ) / FalloffRange, 0.0f, 1.0f ) : 1.0f;
		ExposureSum += SampleExposure;

#if !UE_BUILD_SHIPPING
		if ( bDebugDrawOcclusionTrace )
		{
			const FColor LineColor = FLinearColor::LerpUsingHSV( FLinearColor::Red, FLinearColor::Green, SampleExposure ).ToFColor( false );
			DrawDebugLine( World, SamplePoint, bBlocked ? Hit.Location : UpwindPoint, LineColor, false, -1.0f, 0, 2.0f );
			DrawDebugPoint( World, SamplePoint, 8.0f, LineColor, false, -1.0f );
		}
#endif
	}

	const float ExposureRatio = ExposureSum / SampleCount;

#if !UE_BUILD_SHIPPING
	if ( bDebugDrawOcclusionTrace )
	{
		// 発生源が効いているかの切り分け用
		const FString ModeText = ( OcclusionDirectionMode == EWindDirectionMode::FromSource )
			? FString::Printf( TEXT( "FromSource（Source=%s）" ), WindSourceActor ? *WindSourceActor->GetName() : TEXT( "未設定→Fixedへフォールバック" ) )
			: TEXT( "Fixed" );
		const FString Text = FString::Printf( TEXT( "Wind Exposure: %.0f%%\nOcclusionDirectionMode: %s\nTraceDir=%s" ),
			ExposureRatio * 100.0f, *ModeText, *TraceDir.ToCompactString() );
		DrawDebugString( World, Center + FVector( 0, 0, 50 ), Text, nullptr, FColor::Orange, 0.0f, true );
	}
#endif

	return ExposureRatio;
}

void AWindZone::GenerateDebugFlowSeeds()
{
	DebugFlowSeeds.Reset();
	DebugFlowSeeds.Reserve( DebugFlowArrowCount );

	FRandomStream Stream( GetUniqueID() );
	for ( int32 Index = 0; Index < DebugFlowArrowCount; ++Index )
	{
		// 均一分布させるため sqrt(半径) で単位円上の点を取る
		const float Angle = Stream.FRandRange( 0.0f, 2.0f * PI );
		const float Radius = FMath::Sqrt( Stream.FRandRange( 0.0f, 1.0f ) );

		FWindDebugFlowSeed Seed;
		Seed.LateralUnit = FVector2D( FMath::Cos( Angle ), FMath::Sin( Angle ) ) * Radius;
		Seed.PhaseOffset = Stream.FRandRange( 0.0f, 1.0f );
		DebugFlowSeeds.Add( Seed );
	}
}

#if !UE_BUILD_SHIPPING
void AWindZone::DrawDebugFlowArrows() const
{
	UWorld* World = GetWorld();
	if ( !World || DebugFlowSeeds.IsEmpty() ) return;

	// 風向き軸（ローカル +X）に沿ってエリア幅ぶんループさせながら流す
	const float RangeLength = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.X * 2.0f : SphereRadius * 2.0f;
	if ( RangeLength <= KINDA_SMALL_NUMBER ) return;
	const float HalfRange = RangeLength * 0.5f;

	const float TimeSeconds = World->GetTimeSeconds();
	const FVector Dir = GetActorForwardVector().GetSafeNormal2D();
	const FTransform& ActorTransform = GetActorTransform();

	for ( const FWindDebugFlowSeed& Seed : DebugFlowSeeds )
	{
		const float LocalX = FMath::Fmod( TimeSeconds * DebugFlowArrowSpeed + Seed.PhaseOffset * RangeLength, RangeLength ) - HalfRange;

		FVector LocalPos;
		if ( Shape == EHazardAreaShape::Box )
		{
			LocalPos = FVector( LocalX, Seed.LateralUnit.X * BoxExtent.Y * DebugFlowArrowLateralSpread, Seed.LateralUnit.Y * BoxExtent.Z * DebugFlowArrowLateralSpread );
		}
		else
		{
			// 中心から離れるほど横断面が狭くなる分を反映する
			const float MaxRadiusAtX = FMath::Sqrt( FMath::Max( 0.0f, SphereRadius * SphereRadius - LocalX * LocalX ) );
			LocalPos = FVector( LocalX, Seed.LateralUnit.X * MaxRadiusAtX * DebugFlowArrowLateralSpread, Seed.LateralUnit.Y * MaxRadiusAtX * DebugFlowArrowLateralSpread );
		}

		const FVector WorldPos = ActorTransform.TransformPosition( LocalPos );
		DrawDebugDirectionalArrow( World, WorldPos, WorldPos + Dir * DebugFlowArrowLength, DebugFlowArrowLength * DebugFlowArrowHeadSizeRatio, DebugFlowArrowColor, false, -1.0f, 0, DebugFlowArrowThickness );
	}
}

void AWindZone::DrawDebugLateralGradient() const
{
	if ( !bEnableLateralGradient ) return;

	UWorld* World = GetWorld();
	if ( !World ) return;

	const float HalfWidth = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.Y : SphereRadius;	// 左右（ローカル Y）
	const float HalfLength = ( Shape == EHazardAreaShape::Box ) ? BoxExtent.X : SphereRadius;	// 風向き（ローカル X）
	if ( HalfWidth <= KINDA_SMALL_NUMBER || HalfLength <= KINDA_SMALL_NUMBER ) return;

	const FTransform& ActorTransform = GetActorTransform();

	// 幅方向のレーンを風力倍率（緑=フル〜赤=弱）で色分けし、中央平面へ風向きに沿って引く
	constexpr int32 LaneCount = 21;
	for ( int32 Index = 0; Index < LaneCount; ++Index )
	{
		const float LocalY = FMath::Lerp( -HalfWidth, HalfWidth, float( Index ) / float( LaneCount - 1 ) );
		const FVector WorldSample = ActorTransform.TransformPosition( FVector( 0.0f, LocalY, 0.0f ) );
		const float ScaleAtLane = ComputeLateralGradientScale( WorldSample );
		const FColor LaneColor = FLinearColor::LerpUsingHSV( FLinearColor::Red, FLinearColor::Green, ScaleAtLane ).ToFColor( false );

		const FVector Start = ActorTransform.TransformPosition( FVector( -HalfLength, LocalY, 0.0f ) );
		const FVector End = ActorTransform.TransformPosition( FVector( HalfLength, LocalY, 0.0f ) );
		DrawDebugLine( World, Start, End, LaneColor, false, -1.0f, 0, 2.0f );
	}

	// 減衰帯の内側境界（ここより内側がフル）を白線で強調する
	const float BoundaryY = HalfWidth - LateralGradientWidth;
	if ( LateralGradientWidth > KINDA_SMALL_NUMBER && BoundaryY > 0.0f )
	{
		for ( const float SignedY : { -BoundaryY, BoundaryY } )
		{
			const FVector Start = ActorTransform.TransformPosition( FVector( -HalfLength, SignedY, 0.0f ) );
			const FVector End = ActorTransform.TransformPosition( FVector( HalfLength, SignedY, 0.0f ) );
			DrawDebugLine( World, Start, End, FColor::White, false, -1.0f, 0, 4.0f );
		}
	}
}
#endif

void AWindZone::ReleaseAllWindTargets()
{
	for ( const TWeakObjectPtr<AActor>& Prev : WindAffectedActors )
	{
		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IWindAffectable* Affectable = Cast<IWindAffectable>( PrevActor ) )
			{
				Affectable->OnWindExit();
			}
		}
	}
	WindAffectedActors.Reset();

	// 遠距離ゲート離脱・破棄時に AreaExit を漏らさない
	for ( const TWeakObjectPtr<AActor>& Prev : WindAreaActors )
	{
		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IWindAffectable* Affectable = Cast<IWindAffectable>( PrevActor ) )
			{
				Affectable->OnWindZoneAreaExit();
			}
		}
	}
	WindAreaActors.Reset();
}
