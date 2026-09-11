// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/AntlionCore.h"

#include "PRJ_TIDE_P0/Actors/Prop/BreakableProp.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Utilities/TideProximityGate.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

AAntlionCore::AAntlionCore()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneRoot" ) );
	RootComponent = SceneRoot;

	CollisionComp = CreateDefaultSubobject<USphereComponent>( TEXT( "CollisionComp" ) );
	CollisionComp->SetupAttachment( SceneRoot );
	CollisionComp->SetSphereRadius( 80.0f );
	CollisionComp->SetCollisionEnabled( ECollisionEnabled::QueryOnly );
	// 竜巻の AllDynamicObjects スイープに拾われるため WorldDynamic にする
	// （ObjectType が合っていないと囲い込み検出そのものが発生しない）
	CollisionComp->SetCollisionObjectType( ECC_WorldDynamic );
	CollisionComp->SetCollisionResponseToAllChannels( ECR_Block );
	CollisionComp->SetCollisionResponseToChannel( ECC_Visibility, ECR_Ignore );
	CollisionComp->SetCollisionResponseToChannel( ECC_Camera, ECR_Ignore );

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "MeshComp" ) );
	MeshComp->SetupAttachment( SceneRoot );
	MeshComp->SetCollisionEnabled( ECollisionEnabled::NoCollision );

	// 判定基準点。既定は SceneRoot と同じ位置で、エディタでドラッグして Z オフセットを補正できる
	ProximityOrigin = CreateDefaultSubobject<USceneComponent>( TEXT( "ProximityOrigin" ) );
	ProximityOrigin->SetupAttachment( SceneRoot );

	ProximityVis = CreateDefaultSubobject<USphereComponent>( TEXT( "ProximityVis" ) );
	ProximityVis->SetupAttachment( ProximityOrigin );
	ProximityVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	ProximityVis->SetSphereRadius( ProximityFuseRadius );
	ProximityVis->ShapeColor = FColor::Green;
	ProximityVis->SetHiddenInGame( true );
	ProximityVis->SetVisibility( false );

	// 作動範囲のエディタ可視化（ProximityOrigin 基準・見た目専用）。半径・可視性は OnConstruction で反映する
	ActivationCullVis = CreateDefaultSubobject<USphereComponent>( TEXT( "ActivationCullVis" ) );
	ActivationCullVis->SetupAttachment( ProximityOrigin );
	ActivationCullVis->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	ActivationCullVis->SetGenerateOverlapEvents( false );
	ActivationCullVis->ShapeColor = FColor::Orange;
	ActivationCullVis->SetVisibility( false );
	ActivationCullVis->SetHiddenInGame( true );
}

void AAntlionCore::OnConstruction( const FTransform& Transform )
{
	Super::OnConstruction( Transform );

	if ( ProximityVis )
	{
		ProximityVis->SetSphereRadius( ProximityFuseRadius );
		// Pit 側の見た目メッシュ中心とのズレを目視確認できるよう、エディタ・実行時とも常時表示する
		ProximityVis->SetVisibility( bDebugDraw );
		ProximityVis->SetHiddenInGame( !bDebugDraw );
	}

	if ( ActivationCullVis )
	{
		ActivationCullVis->SetSphereRadius( FMath::Max( 0.0f, ProximityFuseRadius + ActivationCullMargin ) );
		ActivationCullVis->SetVisibility( bDrawActivationCullRange );
	}
}

void AAntlionCore::BeginPlay()
{
	Super::BeginPlay();

	if ( MeshComp && MeshComp->GetNumMaterials() > 0 )
	{
		BlinkMID = MeshComp->CreateAndSetMaterialInstanceDynamic( 0 );
	}
}

void AAntlionCore::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	if ( State == EAntlionCoreState::Destroyed ) return;

	UpdateProximityFuse( DeltaTime );
	ApplyBlinkMaterial( DeltaTime );

	// カウント中は赤、待機中は緑（可視性の制御は OnConstruction 側）
	if ( bDebugDraw && ProximityVis )
	{
		ProximityVis->ShapeColor = bProximityFuseActive ? FColor::Red : FColor::Green;
	}

#if !UE_BUILD_SHIPPING
	if ( bDrawActivationCullRange )
	{
		const FVector GateCenter = ProximityOrigin ? ProximityOrigin->GetComponentLocation() : GetActorLocation();
		const bool bWithinCull = TideProximityGate::IsPlayerWithinDistance( this, GateCenter, ProximityFuseRadius + ActivationCullMargin, CachedPlayerPawn );
		TideProximityGate::DrawActivationRange( this, GateCenter, ProximityFuseRadius + ActivationCullMargin, bWithinCull );
	}
#endif
}

void AAntlionCore::UpdateProximityFuse( float DeltaTime )
{
	if ( ReArmRemaining > 0.0f )
	{
		ReArmRemaining -= DeltaTime;
		return;
	}

	// まだ開始していなければ、範囲内に敵対対象が入った時点でカウントを開始する
	if ( !bProximityFuseActive )
	{
		// プレイヤーが十分離れていれば全 Pawn 走査をスキップする（開始後は離れても継続する）
		const FVector GateCenter = ProximityOrigin ? ProximityOrigin->GetComponentLocation() : GetActorLocation();
		if ( !TideProximityGate::IsPlayerWithinDistance( this, GateCenter, ProximityFuseRadius + ActivationCullMargin, CachedPlayerPawn ) )
		{
			return;
		}

		if ( HasHostileTargetWithin( ProximityFuseRadius ) )
		{
			bProximityFuseActive = true;
			ProximityFuseRemaining = FMath::Max( 0.01f, ProximityFuseDuration );
			State = EAntlionCoreState::Fusing;
		}
		return;
	}

	// 一度開始したら離れても継続する（キャンセルしない）
	ProximityFuseRemaining -= DeltaTime;
	if ( ProximityFuseRemaining <= 0.0f )
	{
		Explode();
	}
}

bool AAntlionCore::HasHostileTargetWithin( float Radius )
{
	if ( Radius <= 0.0f ) return false;

	// 3D 距離の中心は GetActorLocation() ではなく ProximityOrigin。Core 本体を Z オフセットで沈めても、
	// ProximityOrigin をドラッグして望みの高さへ戻せば素直な球判定のまま機能する
	const FVector Center = ProximityOrigin ? ProximityOrigin->GetComponentLocation() : GetActorLocation();

	TArray<AActor*> Pawns;
	UGameplayStatics::GetAllActorsOfClass( this, APawn::StaticClass(), Pawns );

	bool bFound = false;

	for ( AActor* Actor : Pawns )
	{
		if ( !Actor || Actor == this ) continue;

		// GetOwner() には依存しない。this は必ず非null かつ IGenericTeamAgentInterface 未実装＝常に
		// NoTeam 扱いなので、Owner の設定タイミングに左右されず「誰にでも敵対」が確実に成立する
		if ( !TideCombatUtil::IsHostileTo( this, Actor ) ) continue;
		if ( FVector::Dist( Center, Actor->GetActorLocation() ) > Radius ) continue;

		bFound = true;
		break;
	}

	return bFound;
}

void AAntlionCore::Explode()
{
	if ( State == EAntlionCoreState::Destroyed ) return;

	const FVector Center = ProximityOrigin ? ProximityOrigin->GetComponentLocation() : GetActorLocation();

	// 接近起爆と同じ ProximityOrigin 中心の 3D 距離で判定する
	TArray<AActor*> Targets;
	UGameplayStatics::GetAllActorsOfClass( this, APawn::StaticClass(), Targets );

	// 壊れ物は Pawn ではないので上の走査に入らない。専用に集めて同じ判定にかける
	if ( bDamageBreakableProps )
	{
		TArray<AActor*> Props;
		UGameplayStatics::GetAllActorsOfClass( this, ABreakableProp::StaticClass(), Props );
		Targets.Append( Props );
	}

	for ( AActor* Actor : Targets )
	{
		if ( !Actor || Actor == this ) continue;
		// GetOwner() には依存しない（HasHostileTargetWithin と同じ理由）
		if ( !TideCombatUtil::IsHostileTo( this, Actor ) ) continue;
		if ( FVector::Dist( Center, Actor->GetActorLocation() ) > ExplosionRadius ) continue;

		IDamageable* Damageable = Cast<IDamageable>( Actor );
		if ( !Damageable ) continue;

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage = ExplosionDamage;
		DamageInfo.Instigator = this;	// 必ず非null にする（GetOwner() は null の可能性がある）
		DamageInfo.HitReactionTag = HitReactionTag;

		// 壊れ物は既定でプレイヤーの攻撃しか受け付けないので、環境ギミック由来として明示的に許可する
		DamageInfo.bCanBreakProps = bDamageBreakableProps;
		DamageInfo.HitResult.ImpactPoint = Center;
		DamageInfo.HitResult.ImpactNormal = ( Actor->GetActorLocation() - Center ).GetSafeNormal();
		Damageable->ReceiveDamage( DamageInfo );
	}

	if ( ExplosionEffect )
	{
		// 2D ビルボードが地面やメッシュに埋まって見えないよう、カメラ方向へずらす
		FVector SpawnLocation = Center;
		if ( !FMath::IsNearlyZero( ExplosionEffectCameraOffset ) )
		{
			if ( APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager( this, 0 ) )
			{
				const FVector ToCamera = ( CameraManager->GetCameraLocation() - Center ).GetSafeNormal();
				SpawnLocation += ToCamera * ExplosionEffectCameraOffset;
			}
		}
		SpawnLocation.Z += ExplosionEffectZOffset;

		// アセット側がアクター Scale を無視して内部の User Parameter でサイズを決める作りなので、
		// Transform Scale は 1 のままスポーンする
		UNiagaraComponent* SpawnedFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation( this, ExplosionEffect, SpawnLocation );
		if ( SpawnedFX && !ExplosionEffectScaleParameterName.IsNone() )
		{
			SpawnedFX->SetVariableFloat( ExplosionEffectScaleParameterName, ExplosionEffectScale );
		}
	}

	// 自壊せず Idle へ戻し、クールダウン後に再度明滅できるようにする
	bProximityFuseActive = false;
	ProximityFuseRemaining = 0.0f;
	ReArmRemaining = PostExplosionReArmDelay;
	BlinkPhase = 0.0f;
	State = EAntlionCoreState::Idle;
	if ( BlinkMID )
	{
		BlinkMID->SetScalarParameterValue( BlinkParameterName, 0.0f );
	}
}

void AAntlionCore::ApplyBlinkMaterial( float DeltaTime )
{
	if ( !BlinkMID ) return;

	if ( !bProximityFuseActive )
	{
		BlinkPhase = 0.0f;
		BlinkMID->SetScalarParameterValue( BlinkParameterName, 0.0f );
		return;
	}

	// 残り時間が 0 に近づくほど周期を短くして点滅を加速させる
	const float Alpha = FMath::Clamp( ProximityFuseRemaining / FMath::Max( 0.01f, ProximityFuseDuration ), 0.0f, 1.0f );
	const float FastPeriod = FMath::Max( 0.01f, ProximityFuseFastBlinkPeriod );
	const float Period = FMath::Lerp( FastPeriod, FMath::Max( 0.01f, BlinkPeriod ), Alpha );

	BlinkPhase += ( 2.0f * PI * DeltaTime ) / Period;
	const float BlinkValue = 0.5f + 0.5f * FMath::Sin( BlinkPhase );
	BlinkMID->SetScalarParameterValue( BlinkParameterName, BlinkValue );
}

void AAntlionCore::OnWindEnter( const FWindInfluence& Wind )
{
	DestroyCore();
}

void AAntlionCore::DestroyCore()
{
	if ( State == EAntlionCoreState::Destroyed ) return;

	State = EAntlionCoreState::Destroyed;
	bProximityFuseActive = false;

	SetActorEnableCollision( false );
	if ( MeshComp )
	{
		MeshComp->SetVisibility( false );
	}

	// 購読者へ通知してから破棄する（渦の見た目フェードは AAntlionPit 側の「SM_arijigoku」
	// メッシュが担当。詳細は Docs/AntlionGimmick.md 参照）
	OnCoreDestroyed.Broadcast();

	Destroy();
}
