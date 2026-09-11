// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ExCameraActor.h"

#include "Camera/CameraComponent.h"
#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"

AExCameraActor::AExCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CameraModeComponent = CreateDefaultSubobject<UExCameraModeComponent>( TEXT( "CameraModeComponent" ) );
}

void AExCameraActor::ApplyCameraView( const FMinimalViewInfo& ViewInfo )
{
	SetActorLocationAndRotation( ViewInfo.Location, ViewInfo.Rotation );
	if ( UCameraComponent* CamComp = GetCameraComponent() )
	{
		CamComp->SetFieldOfView( ViewInfo.FOV );
		CamComp->SetOrthoWidth( ViewInfo.OrthoWidth );
		CamComp->SetAspectRatio( ViewInfo.AspectRatio );
		CamComp->SetConstraintAspectRatio( ViewInfo.bConstrainAspectRatio );
		CamComp->SetProjectionMode( ViewInfo.ProjectionMode );
		CamComp->PostProcessSettings = ViewInfo.PostProcessSettings;
		CamComp->PostProcessBlendWeight = ViewInfo.PostProcessBlendWeight;
	}
}
