// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/ConductorActorRow.h"
#include "ConductorActorInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UConductorActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * コンダクタの状態変更通知
 * 表示/当たり/TickはPlugin側で設定するので、ここにはそれ以外(AI停止とか)を書く
 */
class CONDUCTOR_API IConductorActorInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Conductor")
	void OnConductorStateChanged(EConductorActorState State);
};
