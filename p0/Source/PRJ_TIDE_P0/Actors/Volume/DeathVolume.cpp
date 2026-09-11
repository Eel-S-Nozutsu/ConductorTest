// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Volume/DeathVolume.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

void ADeathVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	// HPを尽きさせて通常の死亡経路(OnDeath)へ乗せる ※プレイヤー・敵共通
	if (const ATideCharacter* Char = Cast<ATideCharacter>(OtherActor))
	{
		if (UStatusComponent* Status = Char->GetStatusComponent())
		{
			Status->ModifyHP(-TNumericLimits<float>::Max());
		}
	}
}
