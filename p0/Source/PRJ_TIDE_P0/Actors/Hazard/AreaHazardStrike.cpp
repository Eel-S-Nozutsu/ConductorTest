// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardStrike.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardZone.h"
#include "PRJ_TIDE_P0/Components/Combat/HazardDotComponent.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"

AAreaHazardStrike::AAreaHazardStrike()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	WarningVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "WarningVFX" ) );
	WarningVFX->SetupAttachment( RootComponent );
	WarningVFX->bAutoActivate = false;

	StrikeVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "StrikeVFX" ) );
	StrikeVFX->SetupAttachment( RootComponent );
	StrikeVFX->bAutoActivate = false;
}

void AAreaHazardStrike::Activate( const FVector& InStrikeLocation, const FVector& InFloorNormal )
{
	StrikeLocation = InStrikeLocation;
	FloorNormal = InFloorNormal;
	// 見た目（予兆/落雷 VFX）だけを床の法線に合わせて傾ける。当たり判定は常に world Z 方向の縦筒スイープのままで
	// 傾きの影響を受けない（斜面上でも判定範囲を一定に保つため）
	SetActorLocationAndRotation( InStrikeLocation, FRotationMatrix::MakeFromZ( InFloorNormal ).Rotator() );

	Phase = EPhase::Warning;
	ElapsedTime = 0.0f;

	if ( WarningVFX && WarningVFX->GetAsset() )
	{
		// Scale は Activate 前に設定する（オーバーライドパラメータとして初期値に反映される）
		WarningVFX->SetFloatParameter( TEXT( "Scale" ), GetWarningVFXScale() );
		WarningVFX->Activate( true );
	}

	SetActorTickEnabled( true );
}

void AAreaHazardStrike::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	ElapsedTime += DeltaTime;

	switch ( Phase )
	{
	case EPhase::Warning:
	{
#if !UE_BUILD_SHIPPING
		if ( bDebugDraw )
		{
			// 予兆＝落雷と同じ円柱形（色はオレンジ）。見た目のみ床の法線（ActorRotation）に沿わせ、
			// 当たり判定（ApplyStrikeDamage）は world Z スイープのまま
			const FVector Center = GetActorLocation();
			const FVector Up = GetActorUpVector();
			DrawDebugCylinder( GetWorld(), Center, Center + Up * StrikeHeight,
				StrikeRadius, 24, FColor::Orange, false, -1.0f, 0, 6.0f );
		}
#endif
		if ( ElapsedTime >= WarningDuration )
		{
			EnterStrike();
		}
		break;
	}
	case EPhase::Strike:
	{
#if !UE_BUILD_SHIPPING
		if ( bDebugDraw )
		{
			// 落雷＝一瞬のフラッシュ（余韻中は範囲を塗る）。見た目のみ床の法線に沿わせ、判定は world Z のまま
			const FVector Center = GetActorLocation();
			const FVector Up = GetActorUpVector();
			DrawDebugCylinder( GetWorld(), Center, Center + Up * StrikeHeight,
				StrikeRadius, 24, FColor::Cyan, false, -1.0f, 0, 6.0f );
		}
#endif
		if ( ElapsedTime >= StrikeLingerBeforeDestroy )
		{
			Phase = EPhase::Done;
			Destroy();
		}
		break;
	}
	default:
		break;
	}
}

void AAreaHazardStrike::EnterStrike()
{
	Phase = EPhase::Strike;
	ElapsedTime = 0.0f;

	if ( WarningVFX )
	{
		WarningVFX->Deactivate();
	}
	if ( StrikeVFX && StrikeVFX->GetAsset() )
	{
		StrikeVFX->SetFloatParameter( TEXT( "Scale" ), GetStrikeVFXScale() );
		StrikeVFX->Activate( true );
	}

	ApplyStrikeDamage();
	SpawnZone();
}

float AAreaHazardStrike::GetWarningVFXScale() const
{
	return StrikeRadius / FMath::Max( 1.0f, WarningVFXBaseRadius );
}

float AAreaHazardStrike::GetStrikeVFXScale() const
{
	return StrikeRadius / FMath::Max( 1.0f, StrikeVFXBaseRadius );
}

void AAreaHazardStrike::ApplyStrikeDamage()
{
	UWorld* World = GetWorld();
	if ( !World ) return;

	// 竜巻スリップと同じく AllDynamicObjects の短い縦スイープで範囲内アクターを集める（一撃・再アームなし）
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( AreaHazardStrike ), false, this );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( StrikeRadius );
	// 地面から上方向へ StrikeHeight ぶんの円柱状判定（段差上・空中の対象も拾う）
	const FVector SweepStart = StrikeLocation;
	const FVector SweepEnd   = StrikeLocation + FVector( 0, 0, StrikeHeight );

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType( Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, Params );

	TSet<AActor*> Processed;
	for ( const FHitResult& Hit : Hits )
	{
		AActor* Other = Hit.GetActor();
		if ( !Other || Other == this || Other == GetOwner() ) continue;
		if ( Processed.Contains( Other ) ) continue;
		if ( !TideCombatUtil::IsHostileTo( GetOwner(), Other ) ) continue;

		IDamageable* Damageable = Cast<IDamageable>( Other );
		if ( !Damageable ) continue;

		Processed.Add( Other );

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage     = StrikeDamage;
		// 吹き飛びは Instigator から離れる方向へ飛ぶので、中心から外側へ飛ばすため Instigator をこの Strike にする
		// （敵味方判定は上の IsHostileTo が担うので Instigator を変えても影響しない）
		DamageInfo.Instigator     = this;
		DamageInfo.HitReactionTag = HitReactionTag;
		DamageInfo.HitResult      = Hit;

		const EDamageResult Result = Damageable->ReceiveDamage( DamageInfo );

		// 実際にダメージが通った相手のみ演出（無敵・回避では出さない）
		if ( Result != EDamageResult::Hit ) continue;

		// 単発ヒットエフェクト（被弾者位置へ）
		if ( HitEffect )
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation( this, HitEffect, Other->GetActorLocation() );
		}

		// 被弾中ループ演出（対象へアタッチ）。一撃なので DamageAuraDuration 秒だけ出す
		if ( DamageAuraVFX && DamageAuraDuration > 0.0f )
		{
			if ( UHazardDotComponent* Dot = Other->FindComponentByClass<UHazardDotComponent>() )
			{
				Dot->RefreshDamageAura( DamageAuraVFX, DamageAuraDuration );
			}
		}

		// ヒットストップはプレイヤーのみ
		if ( bUseHitStop )
		{
			const APawn* Pawn = Cast<APawn>( Other );
			if ( Pawn && Pawn->IsPlayerControlled() )
			{
				HitStopUtil::ApplyHitStop( Other, HitStopDuration, HitStopDilation );
			}
		}
	}
}

void AAreaHazardStrike::SpawnZone()
{
	if ( !ZoneClass ) return;

	UWorld* World = GetWorld();
	if ( !World ) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();	// 敵味方判定のソースを引き継ぐ（NoTeam 運用）
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAreaHazardZone* Zone = World->SpawnActor<AAreaHazardZone>(
		ZoneClass, StrikeLocation, FRotator::ZeroRotator, SpawnParams );
	if ( Zone )
	{
		// 半径・寿命は Zone 側の設定値をそのまま使う。見た目だけ床の法線に合わせる
		Zone->Activate( -1.0f, -1.0f, FloorNormal );
	}
}
