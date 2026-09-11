// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "AbyssVolume.generated.h"

/**
 * 奈落エリアを表すトリガーボリューム
 * プレイヤーが進入すると落下復帰を要求する
 */
UCLASS()
class PRJ_TIDE_P0_API AAbyssVolume : public ATriggerVolume
{
	GENERATED_BODY()

protected:

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

};
