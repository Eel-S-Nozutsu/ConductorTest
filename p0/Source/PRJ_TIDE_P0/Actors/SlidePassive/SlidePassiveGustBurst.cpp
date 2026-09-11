// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/SlidePassive/SlidePassiveGustBurst.h"

#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

ASlidePassiveGustBurst::ASlidePassiveGustBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DamageVolume = CreateDefaultSubobject<USphereComponent>( TEXT( "DamageVolume" ) );
	DamageVolume->SetSphereRadius( 300.0f );
	// 判定はスイープで行うため Overlap のみ・ブロックしない
	DamageVolume->SetCollisionEnabled( ECollisionEnabled::QueryOnly );
	DamageVolume->SetCollisionObjectType( ECC_WorldDynamic );
	DamageVolume->SetCollisionResponseToAllChannels( ECR_Ignore );
	DamageVolume->SetCollisionResponseToChannel( ECC_Pawn, ECR_Overlap );
	DamageVolume->SetGenerateOverlapEvents( true );
	RootComponent = DamageVolume;

	BurstVFX = CreateDefaultSubobject<UNiagaraComponent>( TEXT( "BurstVFX" ) );
	BurstVFX->SetupAttachment( RootComponent );
	BurstVFX->bAutoActivate = false;
}

void ASlidePassiveGustBurst::Activate( UNiagaraSystem* InVFX, float InRadius, float InDamage, float InLifeTime, float InScale )
{
	LifeTime = InLifeTime;
	ElapsedTime = 0.0f;
	Damage = InDamage;
	LastDamageTimes.Reset();
	LaunchSideByActor.Reset();

	if ( DamageVolume )
	{
		DamageVolume->SetSphereRadius( FMath::Max( 1.0f, InRadius ) );
	}

	// 見た目はタグ引き（NiagaraSystemDataAsset → InVFX）を前提に設定して再生する。
	// コンストラクタで bAutoActivate=false にしているため、ここで明示的に Activate する。
	if ( BurstVFX && InVFX )
	{
		BurstVFX->SetAsset( InVFX );
		BurstVFX->SetFloatParameter( TEXT( "Scale" ), InScale );
		// InLifeTime <= 0 は「外部管理（アクション終了まで持続）」を意味する。VFX 側はループ前提で
		// LifeTime を設定しない。> 0 のときだけ Niagara の LifeTime パラメータを渡す。
		if ( InLifeTime > 0.0f )
		{
			BurstVFX->SetFloatParameter( TEXT( "LifeTime" ), InLifeTime );
		}
		BurstVFX->Activate( true );
	}

	// スポーン直後に範囲ダメージを 1 回だけ適用する
	ApplyBurstDamage();

	SetActorTickEnabled( true );
}

void ASlidePassiveGustBurst::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	ElapsedTime += DeltaTime;

	// 竜巻と同様、存続している間ずっと毎フレーム範囲判定する（チャージアクションで突っ込んだ先の
	// ギミック・敵にも当たるように）。同一対象への多段ヒットは再アーム間隔で防ぐ。
	ApplyBurstDamage();

#if !UE_BUILD_SHIPPING
	if ( bDebugDrawVolume && DamageVolume )
	{
		DrawDebugSphere( GetWorld(), GetActorLocation(), DamageVolume->GetScaledSphereRadius(), 24, FColor::Cyan, false, -1.0f, 0, 2.0f );
	}
#endif

	if ( LifeTime > 0.0f && ElapsedTime >= LifeTime )
	{
		Destroy();
	}
}

void ASlidePassiveGustBurst::ApplyBurstDamage()
{
	if ( bDamageSuspended ) return;		// フェードアウト中などは判定しない
	if ( !DamageVolume ) return;
	UWorld* World = GetWorld();
	if ( !World ) return;

	// 竜巻のスリップダメージと同じく、静止物の初期オーバーラップも取れるよう AllDynamicObjects の
	// 短い縦スイープで範囲内アクターを集める（Pawn / WorldDynamic / Destructible 等。地形は拾わない）。
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( GustBurst ), false, this );
	Params.AddIgnoredActor( GetOwner() );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( DamageVolume->GetScaledSphereRadius() );

	const FVector Center = DamageVolume->GetComponentLocation();
	const FVector SweepStart = Center - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = Center + FVector( 0.0f, 0.0f, 1.0f );

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType( Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, Params );

	const double Now = World->GetTimeSeconds();

	// 同一アクターに複数コンポーネントがヒットしても1回に絞る
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

		// このオーラで死んだ敵は突進方向の斜め前へ弾き飛ばす。左右は「中心軸から十分ずれている敵はその側へ、
		// 中心軸付近の敵はランダム」で決めて正面の集団が一方向へ揃うのを防ぎ、判定は敵ごとに記憶して安定させる
		FVector LaunchDir = FVector::ZeroVector;
		if ( const AActor* InstigatorActor = GetOwner() )
		{
			const FVector Right   = InstigatorActor->GetActorRightVector().GetSafeNormal2D();
			const FVector Forward = InstigatorActor->GetActorForwardVector().GetSafeNormal2D();
			if ( !Right.IsNearlyZero() )
			{
				float Side;
				if ( const float* Stored = LaunchSideByActor.Find( Other ) )
				{
					Side = *Stored;
				}
				else
				{
					// 中心軸からの横方向距離
					const FVector ToOther = Other->GetActorLocation() - InstigatorActor->GetActorLocation();
					const float Lateral = FVector::DotProduct( ToOther, Right );
					if ( FMath::Abs( Lateral ) >= 50.0f)
					{
						Side = Lateral >= 0.0f ? 1.0f : -1.0f;
					}
					else
					{
						// 中心軸から50.0未満の範囲は左右どちらに飛ばすかランダムにしてみる
						Side = FMath::RandBool() ? 1.0f : -1.0f;
					}
					LaunchSideByActor.Add( Other, Side );
				}
				// 横(Side)と前方を角度で合成して斜め前にしてみる
				// 0=真横/45=斜め前/90=真正面
				static constexpr float ForwardAngleDegrees = 45.0f;
				const float AngleRad = FMath::DegreesToRadians( ForwardAngleDegrees );
				LaunchDir = ( Right * Side * FMath::Cos( AngleRad ) + Forward * FMath::Sin( AngleRad ) ).GetSafeNormal2D();
			}
		}

		// 再アーム間隔内なら今回はスキップ（多段ヒット防止）
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
		DamageInfo.ChargeGearTag  = TAG_Charge_Gear3; // 仕様上ギア3でのみ発生
		DamageInfo.bBreaksHaloUnconditionally = true; // パッシブは光輪一撃破壊
		DamageInfo.DeathLaunchDirectionOverride = LaunchDir;

		DamageInfo.bOverrideDeathSpinMode = true;
		DamageInfo.DeathSpinModeOverride  = EDeathRagdollSpinMode::Corkscrew;
		DamageInfo.DeathSpinSpeedOverride = 4500.0f;
		DamageInfo.DeathLaunchForceOverride = 1200.0f;
		DamageInfo.DeathLaunchUpForceOverride = 1200.0f;

		Damageable->ReceiveDamage( DamageInfo );
	}
}
