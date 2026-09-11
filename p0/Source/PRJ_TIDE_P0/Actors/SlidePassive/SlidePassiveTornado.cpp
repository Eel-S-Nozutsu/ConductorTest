// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/SlidePassive/SlidePassiveTornado.h"

#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

ASlidePassiveTornado::ASlidePassiveTornado()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DamageVolume = CreateDefaultSubobject<UCapsuleComponent>( TEXT( "DamageVolume" ) );
	DamageVolume->SetCapsuleSize( BaseRadius, BaseHalfHeight );
	// PC をすり抜けさせるためブロックはせず Overlap のみ
	DamageVolume->SetCollisionEnabled( ECollisionEnabled::QueryOnly );
	DamageVolume->SetCollisionObjectType( ECC_WorldDynamic );
	DamageVolume->SetCollisionResponseToAllChannels( ECR_Ignore );
	DamageVolume->SetCollisionResponseToChannel( ECC_Pawn, ECR_Overlap );
	DamageVolume->SetGenerateOverlapEvents( true );
	RootComponent = DamageVolume;

	TornadoVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "TornadoVFX" ) );
	TornadoVFX->SetupAttachment( RootComponent );
	TornadoVFX->bAutoActivate = false;
}

void ASlidePassiveTornado::Activate( UNiagaraSystem* InVFX, float InScale, float InLifeTime, bool bInIsLarge, float InJumpBoostMultiplier )
{
	LifeTime = InLifeTime;
	ElapsedTime = 0.0f;
	bIsLarge = bInIsLarge;
	JumpBoostMultiplier = InJumpBoostMultiplier;

	// スケールに応じて判定範囲を拡縮
	const float SafeScale = FMath::Max( 0.01f, InScale );
	CurrentScale = SafeScale;
	if ( DamageVolume )
	{
		DamageVolume->SetCapsuleSize( BaseRadius * SafeScale, BaseHalfHeight * SafeScale );
	}

	// 見た目を設定して再生（Niagara 側の Scale / LifeTime パラメータも合わせる）
	if ( TornadoVFX && InVFX )
	{
		TornadoVFX->SetAsset( InVFX );
		TornadoVFX->SetFloatParameter( TEXT( "Scale" ), InScale );
		TornadoVFX->SetFloatParameter( TEXT( "LifeTime" ), InLifeTime );
		TornadoVFX->Activate( true );
	}

	SetActorTickEnabled( true );
}

void ASlidePassiveTornado::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	ElapsedTime += DeltaTime;
	if ( LifeTime > 0.0f && ElapsedTime >= LifeTime )
	{
		Destroy();
		return;
	}

	ApplySlipDamage();
	ApplyWind( DeltaTime );

#if !UE_BUILD_SHIPPING
	if ( bDebugDrawVolume && DamageVolume )
	{
		const FVector Center = GetActorLocation();
		const float   R      = DamageVolume->GetScaledCapsuleRadius();
		const float   HH     = DamageVolume->GetScaledCapsuleHalfHeight();
		DrawDebugCapsule( GetWorld(), Center, HH, R, FQuat::Identity, FColor::Cyan, false, -1.0f, 0, 2.0f );
	}
#endif
}

void ASlidePassiveTornado::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	// 破棄・消滅時は巻き上げ中の対象を解放（敵の AI 再開・落下復帰のため）
	ReleaseAllWindTargets();
	Super::EndPlay( EndPlayReason );
}

void ASlidePassiveTornado::ApplySlipDamage()
{
	if ( !DamageVolume ) return;
	UWorld* World = GetWorld();
	if ( !World ) return;

	// Pawn 以外の IDamageable（壊れ物等の WorldDynamic ブロック判定）にも当てる。OverlapMultiByObjectType では
	// ブロック応答の静止コリジョンを取りこぼすため、攻撃本判定と同じ SweepMultiByObjectType＋AllDynamicObjects を使う
	// （静止物も初期オーバーラップとして拾え、WorldStatic の地形は拾わない）
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( TornadoSlip ), false, this );
	Params.AddIgnoredActor( GetOwner() );

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		DamageVolume->GetScaledCapsuleRadius(), DamageVolume->GetScaledCapsuleHalfHeight() );

	// 静止物の初期オーバーラップを確実に取るため、ごく短い縦スイープにする
	const FVector Center = DamageVolume->GetComponentLocation();
	const FVector SweepStart = Center - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = Center + FVector( 0.0f, 0.0f, 1.0f );

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits, SweepStart, SweepEnd, DamageVolume->GetComponentQuat(),
		ObjQuery, Shape, Params );

	const double Now = World->GetTimeSeconds();

	// 同一アクターに複数コンポーネントがヒットしても1回に絞る
	TSet<AActor*> Processed;
	for ( const FHitResult& Hit : Hits )
	{
		AActor* Other = Hit.GetActor();
		if ( !Other || Other == this || Other == GetOwner() ) continue;
		if ( Processed.Contains( Other ) ) continue;
		if ( !TideCombatUtil::IsHostileTo( GetOwner(), Other ) ) continue;

		UPrimitiveComponent* HitComp = Hit.GetComponent();
		if ( !HitComp || !HitComp->ComponentHasTag( TEXT( "DamageLayer" ) ) ) continue;

		IDamageable* Damageable = Cast<IDamageable>( Other );
		if ( !Damageable ) continue;

		Processed.Add( Other );

		// 再アーム間隔内なら今回はスキップ（スリップの刻み）
		if ( const double* Last = LastDamageTimes.Find( Other ) )
		{
			if ( Now - *Last < DamageReArmInterval ) continue;
		}
		LastDamageTimes.Add( Other, Now );

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage     = Damage;
		DamageInfo.Instigator     = GetOwner();
		DamageInfo.HitReactionTag = HitReactionTag;
		DamageInfo.HitResult      = Hit;
		// 受け手 (浮かない敵) が大小で別リアクションを選べるよう段階を伝える
		DamageInfo.WindTier       = bIsLarge ? EWindDamageTier::Large : EWindDamageTier::Small;
		DamageInfo.bBreaksHaloUnconditionally = true; // パッシブは光輪一撃破壊

		Damageable->ReceiveDamage( DamageInfo );
	}
}

void ASlidePassiveTornado::ApplyWind( float DeltaTime )
{
	if ( !DamageVolume ) return;

	// 影響パラメータを構築（中心＝竜巻位置。高さ上限はスケールを乗せる）
	FWindInfluence Wind;
	Wind.Center              = GetActorLocation();
	Wind.LiftSpeed           = LiftSpeed;
	Wind.PullSpeed           = PullSpeed;
	Wind.bPullAffectsPlayer  = false; // 竜巻はプレイヤーを中心へ引き込まない（敵は従来どおり引き込む）
	Wind.MaxLiftHeight       = BaseMaxLiftHeight * CurrentScale;
	// 範囲カプセルの天井 = 風の柱の上端。受け手 (撃ち返し地雷等) が頂上の基準に使う
	Wind.ColumnTopZ          = GetActorLocation().Z + DamageVolume->GetScaledCapsuleHalfHeight();
	Wind.ColumnRadius        = DamageVolume->GetScaledCapsuleRadius();
	// ジャンプ強化倍率は小／大で固定（見た目スケールとは独立。PlayerParamDA から Activate 経由で受け取る）
	Wind.JumpBoostMultiplier = JumpBoostMultiplier;
	Wind.Source              = this;

	// Pawn だけでなく WorldDynamic の IWindAffectable（撃ち返し地雷等）も巻き込むため、スリップダメージと同じく
	// AllDynamicObjects のスイープで集める（GetOverlappingActors は Pawn 応答に依存し WorldDynamic を取りこぼす）。
	// ジャンプ強化を受ける所有者プレイヤーも対象に含めたいので Owner は除外しない
	UWorld* World = GetWorld();
	if ( !World ) return;

	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( TornadoWind ), false, this );

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		DamageVolume->GetScaledCapsuleRadius(), DamageVolume->GetScaledCapsuleHalfHeight() );
	const FVector QueryCenter = DamageVolume->GetComponentLocation();
	const FVector SweepStart = QueryCenter - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = QueryCenter + FVector( 0.0f, 0.0f, 1.0f );

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits, SweepStart, SweepEnd, DamageVolume->GetComponentQuat(),
		ObjQuery, Shape, Params );

	// 今フレームの影響対象を集めつつ、Enter/Tick を発火
	TSet<TWeakObjectPtr<AActor>> CurrentTargets;
	for ( const FHitResult& Hit : Hits )
	{
		AActor* Other = Hit.GetActor();
		if ( !Other || Other == this ) continue;
		if ( CurrentTargets.Contains( Other ) ) continue;

		IWindAffectable* Affectable = Cast<IWindAffectable>( Other );
		if ( !Affectable ) continue;

		CurrentTargets.Add( Other );

		// 前フレームに含まれていなければ Enter
		if ( !WindAffectedActors.Contains( Other ) )
		{
			Affectable->OnWindEnter( Wind );
		}
		Affectable->OnWindTick( Wind, DeltaTime );
	}

	// 範囲から外れた対象へ Exit を発火
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

	WindAffectedActors = MoveTemp( CurrentTargets );
}

void ASlidePassiveTornado::ReleaseAllWindTargets()
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
}
