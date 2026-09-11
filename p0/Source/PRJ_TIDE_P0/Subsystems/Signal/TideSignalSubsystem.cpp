// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideSignalSubsystem.h"

void UTideSignalSubsystem::BroadcastSignal(FName SignalName)
{
	if (SignalName.IsNone()) return;

	FiredSignals.Add(SignalName);

	if (FTideSignalDelegate* Delegate = SignalDelegates.Find(SignalName))
	{
		Delegate->Broadcast();
	}
}

void UTideSignalSubsystem::ResetSignal(FName SignalName)
{
	if (SignalName.IsNone())
	{
		FiredSignals.Empty();
		return;
	}

	FiredSignals.Remove(SignalName);
}

FDelegateHandle UTideSignalSubsystem::ListenForSignal(FName SignalName, FTideSignalDelegate::FDelegate Callback, bool bFireIfAlreadyBroadcast)
{
	if (SignalName.IsNone() || !Callback.IsBound()) return FDelegateHandle();

	const FDelegateHandle Handle = SignalDelegates.FindOrAdd(SignalName).Add(Callback);

	if (bFireIfAlreadyBroadcast && FiredSignals.Contains(SignalName))
	{
		Callback.Execute();
	}

	return Handle;
}

void UTideSignalSubsystem::RemoveListener(FName SignalName, FDelegateHandle Handle)
{
	if (FTideSignalDelegate* Delegate = SignalDelegates.Find(SignalName))
	{
		Delegate->Remove(Handle);
	}
}
