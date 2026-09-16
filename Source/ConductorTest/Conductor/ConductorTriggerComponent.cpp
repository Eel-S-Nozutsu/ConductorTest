// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorTriggerComponent.h"

#include "ConductorSubsystem.h"
#include "ContentConductor.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UConductorTriggerComponent::UConductorTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UConductorTriggerComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!EventTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: 送るイベントが未設定"), *Owner->GetName());
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents(Primitives);

	int32 BoundNum = 0;
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive->GetGenerateOverlapEvents()) continue;

		Primitive->OnComponentBeginOverlap.AddDynamic(this, &UConductorTriggerComponent::HandleOverlap);
		++BoundNum;
	}

	if (BoundNum == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: オーバーラップを出すコリジョンが無い"), *Owner->GetName());
	}
}

void UConductorTriggerComponent::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bOnce && bFired) return;
	if (!OtherActor || OtherActor == GetOwner()) return;
	if (TargetClass && !OtherActor->IsA(TargetClass)) return;

	const UWorld* World			   = GetWorld();
	UConductorSubsystem* Subsystem = World ? World->GetSubsystem<UConductorSubsystem>() : nullptr;
	UContentConductor* Conductor   = Subsystem ? Subsystem->FindContentConductor(ContentId) : nullptr;

	if (!Conductor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: コンテンツ %s が見つからない"), *GetOwner()->GetName(), *ContentId.ToString());
		return;
	}

	bFired = true;

	Conductor->SendStateTreeEvent(EventTag);
}
