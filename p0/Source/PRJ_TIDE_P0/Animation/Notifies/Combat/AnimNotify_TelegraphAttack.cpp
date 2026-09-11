// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotify_TelegraphAttack.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

void UAnimNotify_TelegraphAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	const FVector AttackDir = Owner->GetActorForwardVector();
	const FVector Origin    = Owner->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	Owner->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(DetectionRadius),
		Params);

	TSet<AEnemyCharacter*> Notified;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Overlap.GetActor()))
		{
			if (!Notified.Contains(Enemy))
			{
				Notified.Add(Enemy);
				Enemy->TriggerPredictiveDodge(AttackDir);
			}
		}
	}
}
