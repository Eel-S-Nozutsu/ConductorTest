// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/HaloHazard.h"

#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

AHaloHazard::AHaloHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// VisualMeshのSMシンプルコリジョンを当たり判定に使う
	// PCをすり抜けさせるためブロックはせずOverlapのみ
	// ECC_WorldDynamicへのOverlapはショックウェーブ (WorldDynamic)
	// からの破砕検知に使う
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VisualMesh->SetCollisionObjectType(ECC_WorldDynamic);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	VisualMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	VisualMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	VisualMesh->SetGenerateOverlapEvents(true);

	// VisualMeshに補正回転を入れた場合でも破片の向きが見た目と一致するようVisualMeshの
	// 子にする
	GCComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GCComp"));
	GCComp->SetupAttachment(VisualMesh);
	GCComp->SetSimulatePhysics(false);
	GCComp->SetVisibility(false);
	// 破片はプレイヤー・カメラをブロックしない
	GCComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GCComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	FieldComp = CreateDefaultSubobject<UFieldSystemComponent>(TEXT("FieldComp"));
	FieldComp->SetupAttachment(RootComponent);
}

void AHaloHazard::BeginPlay()
{
	Super::BeginPlay();

	VisualMesh->OnComponentBeginOverlap.AddDynamic(this, &AHaloHazard::OnHazardOverlap);
}

void AHaloHazard::Deploy(const FVector& StartLocation, const FVector& TargetLocation, float StartDelay)
{
	DeployStart  = StartLocation;
	DeployTarget = TargetLocation;
	DeployApex   = FVector(TargetLocation.X, TargetLocation.Y, TargetLocation.Z + ApexHeight);
	DeployElapsed = 0.0f;
	State = EState::Deploying;

	SetActorLocation(StartLocation);

	if (StartDelay > 0.0f)
	{
		// 発射までは非表示で待機する。接触ダメージはActiveになるまで無効なので当たらない
		VisualMesh->SetVisibility(false);
		GetWorldTimerManager().SetTimer(DeployDelayTimerHandle, this,
			&AHaloHazard::BeginDeployMotion, StartDelay, false);
	}
	else
	{
		BeginDeployMotion();
	}
}

void AHaloHazard::BeginDeployMotion()
{
	VisualMesh->SetVisibility(true);
	SetActorTickEnabled(true);
}

void AHaloHazard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == EState::Deploying)
	{
		DeployElapsed += DeltaTime;

		if (DeployElapsed < RiseDuration)
		{
			const float Alpha = (RiseDuration > 0.0f) ? (DeployElapsed / RiseDuration) : 1.0f;
			SetActorLocation(FMath::Lerp(DeployStart, DeployApex, FMath::Clamp(Alpha, 0.0f, 1.0f)));
		}
		else if (DeployElapsed < RiseDuration + FallDuration)
		{
			const float Alpha = (FallDuration > 0.0f) ? ((DeployElapsed - RiseDuration) / FallDuration) : 1.0f;
			SetActorLocation(FMath::Lerp(DeployApex, DeployTarget, FMath::Clamp(Alpha, 0.0f, 1.0f)));
		}
		else
		{
			SetActorLocation(DeployTarget);
			EnterActive();
		}
		return;
	}

	if (bFading)
	{
		FadeElapsed += DeltaTime;
		// Dissolveは0=表示/1=消滅。経過で0→1へ上げて消す
		const float Alpha = FMath::Clamp(FadeElapsed / FadeDuration, 0.0f, 1.0f);
		for (UMaterialInstanceDynamic* MID : FadeMIDs)
		{
			if (MID) MID->SetScalarParameterValue(FadeParameterName, Alpha);
		}

		if (FadeElapsed >= FadeDuration)
		{
			Destroy();
		}
	}
}

void AHaloHazard::EnterActive()
{
	State = EState::Active;

	// 落下の補間が終わったので静止させる
	SetActorTickEnabled(false);

	if (ActiveLifetime > 0.0f)
	{
		// Shatterはオーバーロードのため関数ポインタが曖昧になる。ラムダでno-arg版を指定する
		GetWorldTimerManager().SetTimer(LifetimeTimerHandle, FTimerDelegate::CreateWeakLambda(this,
			[this]() { Shatter(); }), ActiveLifetime, false);
	}
}

void AHaloHazard::OnHazardOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (State != EState::Active) return;
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;
	if (!TideCombatUtil::IsHostileTo(GetOwner(), OtherActor)) return;

	IDamageable* Damageable = Cast<IDamageable>(OtherActor);
	if (!Damageable) return;

	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastDamageTimes.Find(OtherActor))
	{
		if (Now - *Last < DamageReArmInterval) return;
	}
	LastDamageTimes.Add(OtherActor, Now);

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage     = Damage;
	DamageInfo.HitResult      = SweepResult;
	DamageInfo.Instigator     = GetOwner();
	DamageInfo.HitReactionTag = HitReactionTag;

	Damageable->ReceiveDamage(DamageInfo);
}

void AHaloHazard::Shatter()
{
	Shatter(GetActorLocation(), GetActorForwardVector());
}

void AHaloHazard::Shatter(const FVector& ImpactLocation, const FVector& AttackDirection)
{
	if (State == EState::Shattered) return;
	State = EState::Shattered;

	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);

	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetVisibility(false);

	GCComp->SetVisibility(true);
	GCComp->SetSimulatePhysics(true);

	CachedImpactLocation  = ImpactLocation;
	CachedAttackDirection = AttackDirection.GetSafeNormal2D();
	if (CachedAttackDirection.IsNearlyZero())
	{
		CachedAttackDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	// GCCompのMIDを生成してフェードに使う
	FadeMIDs.Empty();
	for (int32 i = 0; i < GCComp->GetNumMaterials(); ++i)
	{
		UMaterialInstanceDynamic* MID = GCComp->CreateAndSetMaterialInstanceDynamic(i);
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParameterName, 0.0f);
			FadeMIDs.Add(MID);
		}
	}

	// GCのDamage Thresholdを超えるStrainを与えてクラスターを砕く
	URadialFalloff* StrainField = NewObject<URadialFalloff>(this);
	StrainField->Magnitude = BreakStrainMagnitude;
	StrainField->Radius    = BreakFieldRadius;
	StrainField->Position  = ImpactLocation;
	StrainField->Falloff   = EFieldFalloffType::Field_FallOff_None;
	StrainField->MinRange  = 0.0f;
	StrainField->MaxRange  = 1.0f;
	StrainField->Default   = 0.0f;

	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_ExternalClusterStrain,
		nullptr, StrainField);

	// 破片は次の物理ステップでフリーボディになるため、力場は遅らせて加える
	GetWorldTimerManager().SetTimer(ScatterTimerHandle, this,
		&AHaloHazard::ApplyScatterForce, 0.05f, false);

	if (BreakEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakEffect, GetActorLocation());
	}

	if (DebrisLifetime > 0.0f)
	{
		if (FadeDuration > 0.0f)
		{
			const float FadeStartDelay = FMath::Max(0.0f, DebrisLifetime - FadeDuration);
			GetWorldTimerManager().SetTimer(FadeStartTimerHandle, this,
				&AHaloHazard::StartFade, FadeStartDelay, false);
		}
		else
		{
			SetLifeSpan(DebrisLifetime);
		}
	}
}

void AHaloHazard::ApplyScatterForce()
{
	// ラジアル散布の原点を攻撃方向の手前へオフセットし、攻撃方向の先へ破片を飛ばす
	const FVector RadialOrigin = CachedImpactLocation - CachedAttackDirection * BreakFieldRadius;

	URadialVector* ForceField = NewObject<URadialVector>(FieldComp);
	ForceField->Magnitude = BreakScatterForce;
	ForceField->Position  = RadialOrigin;

	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_LinearForce,
		nullptr, ForceField);
}

void AHaloHazard::StartFade()
{
	bFading = true;
	FadeElapsed = 0.0f;
	SetActorTickEnabled(true);
}
