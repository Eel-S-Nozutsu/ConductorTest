// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ConductorCondition.h"

#include "ContentConductor.h"

void UConductorCondition::BeginEvaluation(UContentConductor* InConductor)
{
	Conductor = InConductor;
	bActive	  = true;

	if (const UWorld* World = GetWorld())
	{
		PhaseEnterWorldTime = World->GetTimeSeconds();
	}

	OnEvaluationBegin();
}

void UConductorCondition::EndEvaluation()
{
	if (!bActive) return;

	OnEvaluationEnd();

	bActive	  = false;
	Conductor = nullptr;
}

bool UConductorCondition::Evaluate_Implementation()
{
	return false;
}

void UConductorCondition::OnEvaluationBegin_Implementation()
{
}

void UConductorCondition::OnEvaluationEnd_Implementation()
{
}

float UConductorCondition::GetElapsedTime() const
{
	const UWorld* World = GetWorld();
	if (!World || !bActive) return 0.0f;

	return World->GetTimeSeconds() - PhaseEnterWorldTime;
}

// --- 時間経過 ---

bool UConductorCondition_Elapsed::Evaluate_Implementation()
{
	return GetElapsedTime() >= Delay;
}

#if !UE_BUILD_SHIPPING
FString UConductorCondition_Elapsed::GetDebugText() const
{
	return FString::Printf(TEXT("経過 %.1f / %.1f"), GetElapsedTime(), Delay);
}
#endif
