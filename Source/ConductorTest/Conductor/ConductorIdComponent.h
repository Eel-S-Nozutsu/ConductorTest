// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ConductorIdComponent.generated.h"

/**
 * コンダクタから名前で指定できるようにする
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class CONDUCTORTEST_API UConductorIdComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ActorId;
};
