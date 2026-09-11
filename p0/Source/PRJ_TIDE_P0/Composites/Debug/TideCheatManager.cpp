// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "TideCheatManager.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Composites/Debug/TideDebugCameraController.h"

UTideCheatManager::UTideCheatManager()
{
	// デバッグカメラを R1/L1 で速度変更できる拡張クラスに差し替える
	DebugCameraControllerClass = ATideDebugCameraController::StaticClass();
}

void UTideCheatManager::EnterDebugCamera()
{
	EnableDebugCamera();
}

void UTideCheatManager::EnableDebugCamera()
{
	if ( APlayerController* PC = GetOuterAPlayerController() )
	{
		if ( ATidePlayerCharacter* PlayerChar = Cast<ATidePlayerCharacter>( PC->GetPawn() ) )
		{
			PlayerChar->StopAllMovementAndInputs();
		}

		// UEのEnhancedInputのReleasedが来ない対処
		PC->FlushPressedKeys();
	}

	Super::EnableDebugCamera();
}

void UTideCheatManager::Teleport()
{
	if ( !IsDebugCameraActive() ) return;
	Super::Teleport();
}
