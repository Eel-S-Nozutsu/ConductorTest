// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "ExCameraActor.generated.h"

class UExCameraModeComponent;

// カメラモードを管理するコンポーネントを内包したカメラアクター
UCLASS()
class AExCameraActor : public ACameraActor
{
	GENERATED_BODY()

public:
	AExCameraActor();

	void ApplyCameraView( const FMinimalViewInfo& ViewInfo );
	UExCameraModeComponent* GetCameraModeComponent() const { return CameraModeComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tide|Camera System")
	TObjectPtr<UExCameraModeComponent> CameraModeComponent;
};
