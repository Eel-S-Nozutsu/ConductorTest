// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ExCameraMode.h"

void UExCameraMode::InitializeMode( const FExCameraModeCommonParams& Params )
{
	Priority = Params.Priority;
}

void UExCameraMode::UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo )
{

}
