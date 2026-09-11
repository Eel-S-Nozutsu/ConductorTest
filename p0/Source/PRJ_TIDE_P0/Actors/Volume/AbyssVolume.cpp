// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Volume/AbyssVolume.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/Player/FallRecoveryComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

void AAbyssVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (const ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(OtherActor))
	{
		if (UFallRecoveryComponent* Recovery = Player->GetFallRecoveryComponent())
		{
			Recovery->RequestRecovery();
		}
		return;
	}

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OtherActor))
	{
		if (UStatusComponent* Status = Enemy->GetStatusComponent())
			Status->ModifyHP(-TNumericLimits<float>::Max());
	}
}
