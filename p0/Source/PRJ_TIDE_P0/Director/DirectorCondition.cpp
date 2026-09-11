// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "DirectorCondition.h"

#include "PRJ_TIDE_P0/Director/ContentDirector.h"
#include "PRJ_TIDE_P0/Subsystems/Signal/TideSignalSubsystem.h"

void UDirectorCondition::BeginEvaluation(UContentDirector* InDirector)
{
	Director    = InDirector;
	bEvaluating = true;

	if (const UWorld* World = GetWorld())
	{
		BeginWorldTime = World->GetTimeSeconds();
	}

	OnEvaluationBegin();
}

void UDirectorCondition::EndEvaluation()
{
	if (!bEvaluating) return;

	OnEvaluationEnd();

	bEvaluating = false;
	Director    = nullptr;
}

bool UDirectorCondition::Evaluate_Implementation()
{
	return false;
}

void UDirectorCondition::OnEvaluationBegin_Implementation()
{
}

void UDirectorCondition::OnEvaluationEnd_Implementation()
{
}

float UDirectorCondition::GetElapsedTime() const
{
	const UWorld* World = GetWorld();
	if (!World || !bEvaluating) return 0.0f;

	return World->GetTimeSeconds() - BeginWorldTime;
}

// --- 時間経過 ---

bool UDirectorCondition_Elapsed::Evaluate_Implementation()
{
	return GetElapsedTime() >= Delay;
}

FString UDirectorCondition_Elapsed::GetDebugText() const
{
	return FString::Printf(TEXT("経過 %.1f / %.1f"), GetElapsedTime(), Delay);
}

// --- シグナル発火 ---

void UDirectorCondition_Signal::OnEvaluationBegin_Implementation()
{
	if (!bResetSignalOnBegin || SignalName.IsNone()) return;

	if (UTideSignalSubsystem* Signals = GetWorld() ? GetWorld()->GetSubsystem<UTideSignalSubsystem>() : nullptr)
	{
		Signals->ResetSignal(SignalName);
	}
}

bool UDirectorCondition_Signal::Evaluate_Implementation()
{
	if (SignalName.IsNone()) return false;

	const UWorld* World = GetWorld();
	if (!World) return false;

	const UTideSignalSubsystem* Signals = World->GetSubsystem<UTideSignalSubsystem>();

	return Signals && Signals->HasSignalFired(SignalName);
}

FString UDirectorCondition_Signal::GetDebugText() const
{
	return FString::Printf(TEXT("シグナル %s"), *SignalName.ToString());
}

// --- グループ撃破 ---

void UDirectorCondition_GroupDefeated::OnEvaluationBegin_Implementation()
{
	bSeenAlive = false;
}

bool UDirectorCondition_GroupDefeated::Evaluate_Implementation()
{
	UContentDirector* Owner = GetContentDirector();
	if (!Owner || GroupId.IsNone()) return false;

	const int32 AliveCount = Owner->CountAliveInGroup(GroupId);

	if (AliveCount > AliveThreshold)
	{
		bSeenAlive = true;
		return false;
	}

	return bSeenAlive;
}

FString UDirectorCondition_GroupDefeated::GetDebugText() const
{
	UContentDirector* Owner = GetContentDirector();
	const int32 AliveCount  = Owner ? Owner->CountAliveInGroup(GroupId) : -1;

	return FString::Printf(TEXT("%s 生存 %d <= %d %s"),
		*GroupId.ToString(), AliveCount, AliveThreshold, bSeenAlive ? TEXT("") : TEXT("(未検出)"));
}
