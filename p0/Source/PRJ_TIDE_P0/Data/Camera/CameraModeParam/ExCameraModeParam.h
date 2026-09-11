// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ExCameraModeParam.generated.h"

class UExCameraMode;

UCLASS( BlueprintType )
class UExCameraModeParam : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode" )
	TSubclassOf<UExCameraMode> ModeClass;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera Mode" )
	FExCameraModeCommonParams CommonParams;
};
