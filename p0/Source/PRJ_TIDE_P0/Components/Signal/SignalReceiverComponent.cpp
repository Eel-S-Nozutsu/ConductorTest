// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "SignalReceiverComponent.h"
#include "PRJ_TIDE_P0/Subsystems/Signal/TideSignalSubsystem.h"

USignalReceiverComponent::USignalReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USignalReceiverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ListenSignalName.IsNone()) return;

	if (UTideSignalSubsystem* Signals = GetWorld() ? GetWorld()->GetSubsystem<UTideSignalSubsystem>() : nullptr)
	{
		ListenHandle = Signals->ListenForSignal(ListenSignalName,
			FTideSignalDelegate::FDelegate::CreateUObject(this, &USignalReceiverComponent::HandleSignal),
			bFireIfAlreadyBroadcast);
	}
}

void USignalReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ListenHandle.IsValid())
	{
		if (UTideSignalSubsystem* Signals = GetWorld() ? GetWorld()->GetSubsystem<UTideSignalSubsystem>() : nullptr)
		{
			Signals->RemoveListener(ListenSignalName, ListenHandle);
		}
		ListenHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void USignalReceiverComponent::HandleSignal()
{
	OnSignalReceived.Broadcast();
}
