// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Item/Coin.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ACoin::ACoin()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(40.0f);
	CollisionComp->SetCollisionProfileName(TEXT("Trigger"));
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ACoin::OnOverlapBegin);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACoin::BeginPlay()
{
	Super::BeginPlay();
	SpawnLocation = GetActorLocation();
}

void ACoin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPickedUp)
	{
		// 取得アニメーション：上昇
		PickupElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(PickupElapsed / PickupDuration, 0.0f, 1.0f);
		SetActorLocation(PickupStartLocation + FVector(0.0f, 0.0f, PickupRiseHeight * Alpha));
		MeshComp->AddRelativeRotation(FRotator(0.0f, PickupRotationSpeed * DeltaTime, 0.0f));

		if (PickupElapsed >= PickupDuration)
		{
			SetActorHiddenInGame(true);
			CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SetActorTickEnabled(false);

			GetWorldTimerManager().SetTimer(RespawnCheckTimerHandle, this,
				&ACoin::CheckRespawn, 1.0f, true);
		}
		return;
	}

	// 浮き上下
	const float ZOffset = FMath::Sin(GetWorld()->GetTimeSeconds() * FloatSpeed) * FloatAmplitude;
	SetActorLocation(SpawnLocation + FVector(0.0f, 0.0f, ZOffset));

	// 回転 ※メッシュのみ
	MeshComp->AddRelativeRotation(FRotator(0.0f, RotationSpeed * DeltaTime, 0.0f));
}

void ACoin::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPickedUp) return;
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(this, 0)) return;

	Pickup();
}

void ACoin::Pickup()
{
	bPickedUp = true;
	PickupElapsed = 0.0f;
	PickupTime = GetWorld()->GetTimeSeconds();
	PickupStartLocation = GetActorLocation();

	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACoin::CheckRespawn()
{
	if (GetWorld()->GetTimeSeconds() - PickupTime < RespawnDelay) return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return;

	if (FVector::Dist(SpawnLocation, Player->GetActorLocation()) < RespawnDistance) return;

	// リポップ
	GetWorldTimerManager().ClearTimer(RespawnCheckTimerHandle);

	bPickedUp = false;
	PickupElapsed = 0.0f;

	SetActorLocation(SpawnLocation);
	SetActorHiddenInGame(false);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetActorTickEnabled(true);
}
