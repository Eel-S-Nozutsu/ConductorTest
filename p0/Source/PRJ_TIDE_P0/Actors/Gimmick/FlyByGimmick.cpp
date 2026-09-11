// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "FlyByGimmick.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

AFlyByGimmick::AFlyByGimmick()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TriggerComp = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerComp"));
	TriggerComp->SetupAttachment(SceneRoot);
	TriggerComp->InitSphereRadius(100.0f);
	TriggerComp->SetCollisionProfileName(TEXT("Trigger"));

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(SceneRoot);
	PathSpline->SetClosedLoop(false);

	PerformerMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PerformerMesh"));
	PerformerMesh->SetupAttachment(SceneRoot);
	// 空中を突き抜けて飛ぶ純粋な演出なので当たり判定は持たない
	PerformerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFlyByGimmick::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 配置時に始点へスナップしておき、飛翔開始位置をエディタで確認できるようにする
	PlaceMeshAtDistance(0.0f);
}

void AFlyByGimmick::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerComp)
	{
		TriggerComp->OnComponentBeginOverlap.AddUniqueDynamic(this, &AFlyByGimmick::OnTriggerOverlap);
	}

	// 起動するまで見た目は隠しておく
	if (PerformerMesh)
	{
		PlaceMeshAtDistance(0.0f);
		PerformerMesh->SetHiddenInGame(true);
	}
}

void AFlyByGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlayで張ったオーバーラップデリゲートを解除する
	if (TriggerComp)
	{
		TriggerComp->OnComponentBeginOverlap.RemoveDynamic(this, &AFlyByGimmick::OnTriggerOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void AFlyByGimmick::OnTriggerOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (bTriggered) return;

	if (bTriggerByPlayerOnly && !Cast<ATidePlayerCharacter>(OtherActor)) return;

	Trigger();
}

void AFlyByGimmick::Trigger()
{
	if (bTriggered) return;
	bTriggered = true;

	// 以降は起動しないよう判定を切る(1度きり)
	if (TriggerComp)
	{
		TriggerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!PerformerMesh || !PathSpline) return;

	DistanceAlong = 0.0f;
	bFlying = true;

	// 始点へ配置して表示 → モーション再生(AnimSequenceをループ。AnimBP不要)
	PlaceMeshAtDistance(0.0f);
	PerformerMesh->SetHiddenInGame(false);
	if (MoveAnimation)
	{
		PerformerMesh->PlayAnimation(MoveAnimation, /*bLooping=*/true);
	}
}

void AFlyByGimmick::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bFlying) return;

	if (!PerformerMesh || !PathSpline)
	{
		FinishFlight();
		return;
	}

	const float RouteLength = PathSpline->GetSplineLength();
	DistanceAlong += FlightSpeed * DeltaTime;

	PlaceMeshAtDistance(DistanceAlong);

	if (DistanceAlong >= RouteLength)
	{
		FinishFlight();
	}
}

void AFlyByGimmick::PlaceMeshAtDistance(float Distance)
{
	if (!PerformerMesh || !PathSpline) return;

	const float RouteLength = PathSpline->GetSplineLength();
	const float ClampedDistance = FMath::Clamp(Distance, 0.0f, RouteLength);

	const FVector NewLoc = PathSpline->GetLocationAtDistanceAlongSpline(ClampedDistance, ESplineCoordinateSpace::World);
	if (bOrientToPath)
	{
		const FRotator NewRot = PathSpline->GetDirectionAtDistanceAlongSpline(ClampedDistance, ESplineCoordinateSpace::World).Rotation();
		PerformerMesh->SetWorldLocationAndRotation(NewLoc, NewRot);
	}
	else
	{
		PerformerMesh->SetWorldLocation(NewLoc);
	}
}

void AFlyByGimmick::FinishFlight()
{
	if (!bFlying) return;
	bFlying = false;

	// 終端VFX(メッシュの現在位置で再生)
	if (EndEffect && PerformerMesh)
	{
		UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), EndEffect, PerformerMesh->GetComponentLocation(), PerformerMesh->GetComponentRotation());
		if (Spawned)
		{
			Spawned->SetFloatParameter(TEXT("Scale"), EndEffectScale);
		}
	}

	// 演出完了。トリガー・スプラインごと後始末する(EndEffectは独立生成のため残る)
	Destroy();
}
