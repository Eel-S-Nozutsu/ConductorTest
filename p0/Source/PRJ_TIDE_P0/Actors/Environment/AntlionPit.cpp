// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Environment/AntlionPit.h"

#include "PRJ_TIDE_P0/Actors/Hazard/AntlionCore.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGroundPullAffectable.h"

#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

AAntlionPit::AAntlionPit()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	SphereVis = CreateDefaultSubobject<USphereComponent>( TEXT( "SphereVis" ) );
	SphereVis->SetupAttachment( SceneRoot );
	SphereVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	SphereVis->SetSphereRadius( PitRadius );

	// 作動範囲のエディタ可視化（見た目専用）。半径・可視性は OnConstruction で反映する
	ActivationCullVis = CreateDefaultSubobject<USphereComponent>( TEXT( "ActivationCullVis" ) );
	ActivationCullVis->SetupAttachment( SceneRoot );
	ActivationCullVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	ActivationCullVis->SetGenerateOverlapEvents( false );
	ActivationCullVis->ShapeColor = FColor::Orange;
	ActivationCullVis->SetVisibility( false );
	ActivationCullVis->SetHiddenInGame( true );
}

void AAntlionPit::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );
	if ( SphereVis )
	{
		SphereVis->SetSphereRadius( PitRadius );
	}

	// 作動範囲＝PitRadius＋マージンの球。エディタで bDrawActivationCullRange のとき表示
	if ( ActivationCullVis )
	{
		ActivationCullVis->SetSphereRadius( FMath::Max( 0.0f, PitRadius + ActivationCullMargin ) );
		ActivationCullVis->SetVisibility( bDrawActivationCullRange );
	}
}

void AAntlionPit::BeginPlay()
{
	Super::BeginPlay();

	// VortexMesh が未設定なら、BP 側で直接追加された StaticMeshComponent（例："SM_arijigoku"）を自動解決する
	if ( !VortexMesh )
	{
		VortexMesh = FindComponentByClass<UStaticMeshComponent>();
	}

	if ( AntlionCore )
	{
		AntlionCore->OnCoreDestroyed.AddUObject( this, &AAntlionPit::HandleCoreDestroyed );
	}
}

void AAntlionPit::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	// BeginPlay で購読した AntlionCore の破壊デリゲートを解除する。レベル破棄時は Pit / Core の破棄順が
	// 不定なため、Core が先に生きていて後から Broadcast されても解放済みの Pit を叩かないようにしておく
	if ( AntlionCore )
	{
		AntlionCore->OnCoreDestroyed.RemoveAll( this );
	}

	ReleaseAllTargets();
	Super::EndPlay( EndPlayReason );
}

void AAntlionPit::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// 遠距離ゲート：プレイヤーが引き込み範囲から十分離れていれば球オーバーラップ走査をスキップする
	const bool bWithinCull = TideProximityGate::IsPlayerWithinDistance( this, GetActorLocation(), PitRadius + ActivationCullMargin, CachedPlayerPawn );
	if ( bWithinCull )
	{
		ApplyPull( DeltaTime );
	}
	else
	{
		// ゲート外の間は引き込み中の対象を解放しておく（MOVE_Flying 等に取り残さない）
		ReleaseAllTargets();
	}

	if ( !bVortexActive )
	{
		UpdateVortexFade( DeltaTime );
	}

#if !UE_BUILD_SHIPPING
	if ( bDrawActivationCullRange )
	{
		TideProximityGate::DrawActivationRange( this, GetActorLocation(), PitRadius + ActivationCullMargin, bWithinCull );
	}

	if ( bDebugDraw )
	{
		const FColor Color = bVortexActive ? FColor::Orange : FColor::Silver;
		DrawDebugCircle( GetWorld(), GetActorLocation(), PitRadius, 48, Color, false, -1.0f, 0, 3.0f,
			FVector( 1.0f, 0.0f, 0.0f ), FVector( 0.0f, 1.0f, 0.0f ), false );
	}
#endif
}

void AAntlionPit::ApplyPull( float DeltaTime )
{
	if ( !bVortexActive || DeltaTime <= 0.0f )
	{
		ReleaseAllTargets();
		return;
	}

	const FVector Center = GetActorLocation();

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		Center,
		PitRadius,
		{ UEngineTypes::ConvertToObjectType( ECC_Pawn ) },
		nullptr,
		{ this },
		OverlapActors );

	TSet<TWeakObjectPtr<AActor>> CurrentlyPulled;

	for ( AActor* Actor : OverlapActors )
	{
		if ( !Actor ) continue;
		if ( !bAffectEnemies && Cast<AEnemyCharacter>( Actor ) ) continue;

		// 地面の吸い込みなので、空中にいる間（ジャンプ中等）は影響させない
		const ACharacter* Character = Cast<ACharacter>( Actor );
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if ( !Movement || !Movement->IsMovingOnGround() ) continue;

		// すり鉢メッシュ（SM_arijigoku＝VortexMesh）を踏んでいるときだけ吸い込む。
		// 円内の上階など別の床に乗っている場合は接地床が一致しないので除外する
		if ( bRequireStandingOnPitMesh && VortexMesh )
		{
			if ( Movement->CurrentFloor.HitResult.GetComponent() != VortexMesh ) continue;
		}

		IGroundPullAffectable* Affectable = Cast<IGroundPullAffectable>( Actor );
		if ( !Affectable ) continue;

		FVector ToCenter = Center - Actor->GetActorLocation();
		ToCenter.Z = 0.0f;
		const float Distance = ToCenter.Size();
		if ( Distance <= KINDA_SMALL_NUMBER ) continue;

		const FVector InwardDir = ToCenter / Distance;
		// 渦の回転方向。CrossProduct は反交換なので引数順の入れ替えで逆回転にできる
		const FVector TangentDir = FVector::CrossProduct( InwardDir, FVector::UpVector );

		// 中心に近いほど引き込み・渦巻きが強くなる（縁は弱い引き込み・強い渦巻き、中心はその逆で吸い込む）
		const float Alpha = FMath::Clamp( Distance / FMath::Max( 1.0f, PitRadius ), 0.0f, 1.0f );
		const float PullSpeed = FMath::Lerp( PullSpeedNear, PullSpeedFar, Alpha );
		const float SwirlSpeed = FMath::Lerp( SwirlSpeedNear, SwirlSpeedFar, Alpha );

		FGroundPullInfluence Pull;
		Pull.Center = Center;
		Pull.PullVelocity = InwardDir * PullSpeed + TangentDir * SwirlSpeed;
		Pull.Source = this;

		TWeakObjectPtr<AActor> WeakActor( Actor );
		if ( !PulledActors.Contains( WeakActor ) )
		{
			Affectable->OnGroundPullEnter( Pull );
		}
		Affectable->OnGroundPullTick( Pull, DeltaTime );
		CurrentlyPulled.Add( WeakActor );
	}

	// 前フレームまで引き込み中だったが今フレーム外れた対象へ Exit を通知
	for ( const TWeakObjectPtr<AActor>& Prev : PulledActors )
	{
		if ( CurrentlyPulled.Contains( Prev ) ) continue;

		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IGroundPullAffectable* Affectable = Cast<IGroundPullAffectable>( PrevActor ) )
			{
				Affectable->OnGroundPullExit();
			}
		}
	}

	PulledActors = MoveTemp( CurrentlyPulled );
}

void AAntlionPit::ReleaseAllTargets()
{
	for ( const TWeakObjectPtr<AActor>& Prev : PulledActors )
	{
		if ( AActor* PrevActor = Prev.Get() )
		{
			if ( IGroundPullAffectable* Affectable = Cast<IGroundPullAffectable>( PrevActor ) )
			{
				Affectable->OnGroundPullExit();
			}
		}
	}
	PulledActors.Empty();
}

void AAntlionPit::HandleCoreDestroyed()
{
	bVortexActive = false;
	VortexFadeElapsed = 0.0f;
	ReleaseAllTargets();
}

void AAntlionPit::UpdateVortexFade( float DeltaTime )
{
	if ( !VortexMesh ) return;

	const float Duration = FMath::Max( 0.01f, VortexFadeDuration );
	if ( VortexFadeElapsed >= Duration ) return;

	VortexFadeElapsed = FMath::Min( VortexFadeElapsed + DeltaTime, Duration );
	const float Alpha = VortexFadeElapsed / Duration;
	VortexMesh->SetScalarParameterValueOnMaterials( ArijigokuParameterName, 1.0f - Alpha );
}
