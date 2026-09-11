// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "DeathVolume.generated.h"

/**
 * 触れたキャラを即死させるトリガーボリューム。落とし穴・奈落の底に置く。
 * 触れると復帰するAAbyssVolumeの対。
 */
UCLASS()
class PRJ_TIDE_P0_API ADeathVolume : public ATriggerVolume
{
	GENERATED_BODY()

protected:

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

};
