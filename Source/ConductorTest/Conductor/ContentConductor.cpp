// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentConductor.h"

#include "Conductor/ConductorModule.h"
#include "Conductor/ConductorPhaseAction.h"
#include "Conductor/ConductorCondition.h"
#include "Conductor/ContentConductorRow.h"
#include "Conductor/ContentConductorPhaseRow.h"

void UContentConductor::StartConductor(FName InContentId, const FContentConductorRow& Row)
{
	ContentId = InContentId;

	PhaseTable = Row.PhaseTable.LoadSynchronous();

	for (const TSubclassOf<UContentConductorModule>& ModuleClass : Row.Modules)
	{
		if (!ModuleClass) continue;

		UContentConductorModule* Module = NewObject<UContentConductorModule>(this, ModuleClass);
		Modules.Add(Module);
	}

	bStarted = true;

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->StartModule();
	}

	EnterPhase(Row.InitialPhase);
}

void UContentConductor::StopConductor()
{
	if (!bStarted) return;

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		if (CurrentPhase.IsValid())
		{
			//Module->ExitPhase(CurrentPhase);
		}
		Module->StopModule();
	}

	//DestroyAllSpawned();

	Modules.Reset();
	bStarted = false;
}

void UContentConductor::TickConductor(float DeltaSeconds, float EvaluateInterval)
{
	if (!bStarted) return;

	if (bPhasePending)
	{
		bPhasePending = false;
		EnterPhase(PendingPhase);
	}

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->TickModule(DeltaSeconds);
	}

	EvaluateAccumulator += DeltaSeconds;
	if (EvaluateAccumulator < EvaluateInterval) return;

	EvaluateAccumulator = 0.0f;
	EvaluateTransitions();
}

void UContentConductor::EnterPhase(FName NewPhase)
{
	const FName OldPhase = CurrentPhase;

	// 1. 現フェーズの後始末
	ClearTransitions();

	if (!OldPhase.IsNone())
	{
		for (const TObjectPtr<UContentConductorModule>& Module : Modules)
		{
			Module->ExitPhase(OldPhase);
		}
	}

	CurrentPhase = NewPhase;

	if (NewPhase.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: 遷移先のフェーズが未設定"), *ContentId.ToString());
		return;
	}

	const FContentConductorPhaseRow* PhaseRow = FindPhaseRow(NewPhase);
	if (!PhaseRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: フェーズ %s の行がDTに無い"), *ContentId.ToString(), *NewPhase.ToString());
		return;
	}

	// 2. アクターのポップ状態
	//ApplyActorsForPhase(NewPhase);

	// 3. フェーズ開始のアクション実行
	for (const TSubclassOf<UConductorPhaseAction>& ActionClass : PhaseRow->EntryActions)
	{
		if (!ActionClass) continue;

		UConductorPhaseAction* Action = NewObject<UConductorPhaseAction>(this, ActionClass);
		Action->Execute(this);
	}

	// 4. Moduleへ通知
	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->EnterPhase(NewPhase);
	}
}

void UContentConductor::EvaluateTransitions()
{
	// 配列順=優先度とする 最初にtrueになったConditionを採用
	/*for (const FDirectorActiveTransition& Transition : ActiveTransitions)
	{
		if (!Transition.Condition || !Transition.Condition->Evaluate()) continue;

		const FName NextPhase = Transition.NextPhase;

		EnterPhase(NextPhase);
		return;
	}*/
}

void UContentConductor::ClearTransitions()
{
	/*for (const FDirectorActiveTransition& Transition : ActiveTransitions)
	{
		if (Transition.Condition)
		{
			Transition.Condition->EndEvaluation();
		}
	}

	ActiveTransitions.Reset();*/
}

const FContentConductorPhaseRow* UContentConductor::FindPhaseRow(FName Phase) const
{
	if (!PhaseTable) return nullptr;

	return PhaseTable->FindRow<FContentConductorPhaseRow>(Phase, TEXT("UContentConductor::FindPhaseRow"), false);
}
