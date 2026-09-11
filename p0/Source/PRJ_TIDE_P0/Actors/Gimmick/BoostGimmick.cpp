// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "BoostGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"

ABoostGimmick::ABoostGimmick()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	SetRootComponent( SceneRoot );

	TriggerComp = CreateDefaultSubobject<USphereComponent>( TEXT( "TriggerComp" ) );
	TriggerComp->SetupAttachment( SceneRoot );
	TriggerComp->InitSphereRadius( 80.0f );
	TriggerComp->SetCollisionProfileName( TEXT( "Trigger" ) );

	AmbientVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "AmbientVFX" ) );
	AmbientVFX->SetupAttachment( SceneRoot );
	AmbientVFX->bAutoActivate = true;
}

void ABoostGimmick::BeginPlay()
{
	Super::BeginPlay();

	if ( TriggerComp )
	{
		TriggerComp->OnComponentBeginOverlap.AddDynamic( this, &ABoostGimmick::OnTriggerOverlap );
	}

	SetActiveState( true );
}

void ABoostGimmick::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	if ( TriggerComp )
	{
		TriggerComp->OnComponentBeginOverlap.RemoveDynamic( this, &ABoostGimmick::OnTriggerOverlap );
	}

	Super::EndPlay( EndPlayReason );
}

void ABoostGimmick::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	// ここで反映することでエディタ配置時もプレビュー表示される
	if ( AmbientVFX )
	{
		AmbientVFX->SetAsset( AmbientEffect );
		AmbientVFX->SetFloatParameter( TEXT( "Scale" ), AmbientEffectScale );
	}
}

void ABoostGimmick::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	if ( !bIsActive ) return;

#if !UE_BUILD_SHIPPING
	if ( bDebugDraw && TriggerComp )
	{
		DrawDebugBoostVisual();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void ABoostGimmick::DrawDebugBoostVisual() const
{
	UWorld* World = GetWorld();
	if ( !World || !TriggerComp ) return;

	// 負荷を抑えるため緑丸を 1 つ描くだけにする
	const FVector Center = TriggerComp->GetComponentLocation();
	const float Radius = TriggerComp->GetScaledSphereRadius();

	static const FColor GreenColor = FLinearColor( 0.4f, 1.0f, 0.1f, 1.0f ).ToFColor( false );
	DrawDebugSphere( World, Center, Radius, 16, GreenColor, false, -1.0f, 0, 1.0f );
}
#endif

void ABoostGimmick::OnTriggerOverlap( UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/ )
{
	if ( !bIsActive ) return;

	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>( OtherActor );
	if ( !Player ) return;

	Collect( Player );
}

void ABoostGimmick::Collect( ATidePlayerCharacter* Player )
{
	// 空中取得では空中用の速度・持続時間を使う（未設定なら地上と同じ）
	const bool bAirPickup = Player->IsFalling();
	const float EffectiveMaxSpeed = ( bAirPickup && BoostAirMaxSpeed > 0.0f ) ? BoostAirMaxSpeed : BoostMaxSpeed;
	const float EffectiveDuration = bAirPickup ? BoostAirDuration : BoostDuration;

	// 突進開始モーションは地上の BoostDuration を「再生速度1」の基準とし、持続時間が変わったぶんだけ
	// 再生速度をスケールする（例: 基準0.7 / 空中1.0 → 0.7倍速）
	const float StartMontagePlayRate = ( EffectiveDuration > 0.0f && BoostDuration > 0.0f )
		? ( BoostDuration / EffectiveDuration )
		: 1.0f;

	// トリガー球はどの位置で入っても取得するので、触れた縁がそのまま開始位置になると飛距離が毎回変わる。
	// 加速前に中心へ寄せてブレをなくす
	if ( bMoveToCenterOnCollect && TriggerComp )
	{
		MovePlayerToCenterSafely( Player, bAirPickup );
	}

	// 方向は取得時のプレイヤーの向き＝プレイヤー側で解決する
	Player->BeginBoostDash( EffectiveMaxSpeed, EffectiveDuration, BoostVelocityCap, BoostAirZUpSpeed, StartMontagePlayRate );

	// 神技ゲージの加算量はプレイヤーパラメータ側で持つ。
	// エネルギー玉はこの経路だけ大きく・濃くして見やすくする
	if ( const UTidePlayerParamDataAsset* Params = Player->PlayerParamData )
	{
		Player->AddGodActionGauge(
			Params->GodActionGaugeGainOnBoostGimmick,
			Player->GetActorLocation(),
			Params->GodGaugeOrbBoostRadiusScale,
			Params->GodGaugeOrbBoostAlphaScale,
			/*bAnchorOrbToOwner=*/true );	// ブースト直後は高速移動するので発生源をプレイヤー追従にする
	}

	if ( CollectEffect )
	{
		UNiagaraComponent* SpawnedCollect = UNiagaraFunctionLibrary::SpawnSystemAtLocation( GetWorld(), CollectEffect, GetActorLocation(), GetActorRotation() );
		if ( SpawnedCollect )
		{
			SpawnedCollect->SetFloatParameter( TEXT( "Scale" ), CollectEffectScale );
		}
	}

	SetActiveState( false );	// 常時表示エフェクトもここで止まる

	if ( bRespawn && RespawnCooldown > 0.0f )
	{
		GetWorldTimerManager().SetTimer( RespawnTimerHandle, this, &ABoostGimmick::Respawn, RespawnCooldown, false );
	}
	else
	{
		Destroy();
	}
}

void ABoostGimmick::MovePlayerToCenterSafely( ATidePlayerCharacter* Player, bool bAirPickup )
{
	UWorld* World = GetWorld();
	if ( !World || !Player || !TriggerComp ) return;

	UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if ( !Capsule ) return;

	const FVector CurrentLoc = Player->GetActorLocation();
	const FVector Center = TriggerComp->GetComponentLocation();

	// XY は常に中心へ寄せる。Z は地上取得では維持（床めり込み/浮き防止）、
	// 空中取得のみ中心へスナップして縦アークも一定化する
	FVector TargetLoc = CurrentLoc;
	TargetLoc.X = Center.X;
	TargetLoc.Y = Center.Y;
	if ( bAirPickup )
	{
		TargetLoc.Z = Center.Z;
	}

	// 中心へ瞬間移動する際、間に地形があると単純移動では貫通して床下へ抜ける。カプセル形状でスイープし、
	// 地形に当たればその手前で留める（判定対象は地形のみで、敵 Pawn では止めない）
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule( CapsuleRadius, CapsuleHalfHeight );

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery( ECC_WorldStatic );
	ObjectParams.AddObjectTypesToQuery( ECC_WorldDynamic );

	FCollisionQueryParams QueryParams( FName( TEXT( "BoostGimmickCenterSnap" ) ), false, Player );
	QueryParams.AddIgnoredActor( this );

	FHitResult SweepHit;
	if ( World->SweepSingleByObjectType( SweepHit, CurrentLoc, TargetLoc, FQuat::Identity, ObjectParams, CapsuleShape, QueryParams ) )
	{
		// 開始時点で既にめり込んでいる場合は動かさない（安全側）
		if ( !SweepHit.bStartPenetrating )
		{
			TargetLoc = SweepHit.Location;
		}
		else
		{
			TargetLoc = CurrentLoc;
		}
	}

	Player->SetActorLocation( TargetLoc, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics );
}

void ABoostGimmick::Respawn()
{
	SetActiveState( true );
}

void ABoostGimmick::SetActiveState( bool bActive )
{
	bIsActive = bActive;

	if ( TriggerComp )
	{
		// 非表示中は判定を切る（オーバーラップ中に復活しても即再取得させない意図も兼ねる）
		TriggerComp->SetCollisionEnabled( bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision );
	}
	if ( AmbientVFX )
	{
		if ( bActive )
		{
			AmbientVFX->Activate( true );
		}
		else
		{
			AmbientVFX->Deactivate();
		}
	}
}
