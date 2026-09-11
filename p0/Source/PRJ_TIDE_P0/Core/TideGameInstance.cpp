// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Core/TideGameInstance.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "GameFramework/PlayerController.h"

void UTideGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTideGameInstance::ApplyPendingJump);
}

void UTideGameInstance::Shutdown()
{
	if (UTideGameSettings* Settings = UTideGameSettings::Get())
	{
		Settings->SaveSettings();
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Shutdown();
}

void UTideGameInstance::SetPendingJump(FVector Location, float Yaw)
{
	PendingJump = FPendingJumpInfo{ Location, Yaw };
}

void UTideGameInstance::ApplyPendingJump(UWorld* World)
{
	if (!PendingJump.IsSet()) return;

	FPendingJumpInfo Info = PendingJump.GetValue();
	PendingJump.Reset();

	// PostLoadMapWithWorld発火時点ではPawnがまだ存在しないため短いタイマーで待つ
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [World, Info]()
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (!PC) return;
			if (APawn* Pawn = PC->GetPawn())
			{
				const FRotator NewRot(0.0f, Info.Yaw, 0.0f);
				Pawn->SetActorLocation(Info.Location);
				Pawn->SetActorRotation(NewRot);
				PC->SetControlRotation(NewRot);
			}
		}),
		0.1f, false);
}
