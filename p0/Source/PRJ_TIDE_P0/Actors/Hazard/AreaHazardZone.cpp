// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/AreaHazardZone.h"

#include "NiagaraComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Actors/SlidePassive/SlidePassiveTornado.h"
#include "PRJ_TIDE_P0/Components/Combat/HazardDotComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/HazardDotSpec.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

AAreaHazardZone::AAreaHazardZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	ZoneVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "ZoneVFX" ) );
	ZoneVFX->SetupAttachment( RootComponent );
	ZoneVFX->bAutoActivate = false;
}

void AAreaHazardZone::Activate( float InRadius, float InZoneDuration, const FVector& InFloorNormal )
{
	if ( InRadius > 0.0f )        Radius = InRadius;
	if ( InZoneDuration >= 0.0f ) ZoneDuration = InZoneDuration;

	ElapsedTime = 0.0f;

	// 床の法線に見た目だけ合わせる（当たり判定は球オーバーラップなので回転の影響を受けない）。
	// RootComponent にアタッチ済みの ZoneVFX もこの回転に追従する
	SetActorRotation( FRotationMatrix::MakeFromZ( InFloorNormal ).Rotator() );

	// 見た目アセットが入っていれば再生（今は未設定でもデバッグ描画で成立確認できる）
	if ( ZoneVFX && ZoneVFX->GetAsset() )
	{
		// Scale は Activate 前に設定する（オーバーライドパラメータとして初期値に反映される）
		ZoneVFX->SetFloatParameter( TEXT( "Scale" ), GetZoneVFXScale() );
		ZoneVFX->Activate( true );
	}

	SetActorTickEnabled( true );
}

float AAreaHazardZone::GetZoneVFXScale() const
{
	return Radius / FMath::Max( 1.0f, ZoneVFXBaseRadius );
}

void AAreaHazardZone::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// 竜巻に吹き飛ばされ中：DoT・寿命処理は行わず、残粒子のフェードだけ待って破棄する
	if ( bDissipating )
	{
		DissipateElapsed += DeltaTime;

#if !UE_BUILD_SHIPPING
		if ( bDebugDraw )
		{
			// 消滅中は色を変えて区別する
			DrawDebugSphere( GetWorld(), GetActorLocation(), Radius, 24, FColor::Purple, false, -1.0f, 0, 2.0f );
		}
#endif

		if ( DissipateElapsed >= WindDissipateTime )
		{
			Destroy();
		}
		return;
	}

	ElapsedTime += DeltaTime;

	// 遠距離ゲート：プレイヤーが床の範囲から十分離れていれば球オーバーラップ走査をスキップする
	const bool bWithinCull = TideProximityGate::IsPlayerWithinDistance( this, GetActorLocation(), Radius + ActivationCullMargin, CachedPlayerPawn );
	if ( bWithinCull )
	{
		// 竜巻を検出したら風消滅へ移行（この場合 DoT はリフレッシュしない）
		if ( RefreshDotInArea() )
		{
			return;
		}
	}

#if !UE_BUILD_SHIPPING
	if ( bDrawActivationCullRange )
	{
		TideProximityGate::DrawActivationRange( this, GetActorLocation(), Radius + ActivationCullMargin, bWithinCull );
	}

	if ( bDebugDraw )
	{
		// 判定と同じ球を描画（接地フィルタは描画では表現しない）
		DrawDebugSphere( GetWorld(), GetActorLocation(), Radius, 24, FColor::Yellow, false, -1.0f, 0, 2.0f );
	}
#endif

	if ( ZoneDuration > 0.0f && ElapsedTime >= ZoneDuration )
	{
		// ループVFX はアーティスト指示どおり非表示時に Deactivate してから破棄する
		if ( ZoneVFX )
		{
			ZoneVFX->Deactivate();
		}
		Destroy();
	}
}

bool AAreaHazardZone::RefreshDotInArea()
{
	UWorld* World = GetWorld();
	if ( !World ) return false;

	// 動的オブジェクト（Pawn / WorldDynamic 等）を球オーバーラップで集める（初期オーバーラップもそのまま取れる）
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( AreaHazardZone ), false, this );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( Radius );
	const FVector Center = GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType( Overlaps, Center, FQuat::Identity, ObjQuery, Shape, Params );

	// 同一アクターに複数コンポーネントがヒットしても1回に絞る
	TSet<AActor*> Processed;
	for ( const FOverlapResult& Overlap : Overlaps )
	{
		AActor* Other = Overlap.GetActor();
		if ( !Other || Other == this || Other == GetOwner() ) continue;

		// 竜巻が範囲に触れたら床を吹き飛ばす（敵味方判定より前＝プレイヤーの竜巻でも消える）。
		// 竜巻の DamageVolume は WorldDynamic なのでこの AllDynamicObjects オーバーラップに乗る
		if ( bErasableByWind && Cast<ASlidePassiveTornado>( Other ) )
		{
			BeginWindDissipate();
			return true;
		}

		if ( Processed.Contains( Other ) ) continue;
		if ( !TideCombatUtil::IsHostileTo( GetOwner(), Other ) ) continue;

		// しびれ床は接地している対象だけに効く（ジャンプ・浮遊中は効かない）。
		// キャラクター以外は接地判定できないため対象外
		const ACharacter* Character = Cast<ACharacter>( Other );
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if ( !Movement || !Movement->IsMovingOnGround() ) continue;

		UHazardDotComponent* Dot = Other->FindComponentByClass<UHazardDotComponent>();
		if ( !Dot ) continue;

		Processed.Add( Other );

		// 刻みは対象側が持つ。Zone は毎フレーム仕様を渡してリフレッシュするだけ
		FHazardDotSpec Spec;
		Spec.TypeTag         = HazardDotTypeTag;
		Spec.DamagePerTick   = Damage;
		Spec.TickInterval    = DamageInterval;
		Spec.HitReactionTag  = HitReactionTag;
		Spec.HitEffect       = HitEffect;
		Spec.DamageAuraVFX   = DamageAuraVFX;
		Spec.bUseHitStop     = bUseHitStop;
		Spec.HitStopDuration = HitStopDuration;
		Spec.HitStopDilation = HitStopDilation;
		Spec.Instigator      = GetOwner();
		Dot->RefreshDot( Spec );
	}

	return false;
}

void AAreaHazardZone::BeginWindDissipate()
{
	if ( bDissipating ) return;

	bDissipating = true;
	DissipateElapsed = 0.0f;

	// 吹き飛ばされた瞬間から DoT は止まる（以降 RefreshDot を呼ばないため対象側の RefreshGrace 経過で刻みが停止）。
	// ループVFX はアーティスト指示どおり Deactivate（残粒子は WindDissipateTime ぶんフェードしてから Destroy）
	if ( ZoneVFX )
	{
		ZoneVFX->Deactivate();
	}
}
