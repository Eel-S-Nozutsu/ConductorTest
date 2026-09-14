// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentConductor.h"

#include "Conductor/ConductorModule.h"
#include "Conductor/ConductorPhaseAction.h"
#include "Conductor/ConductorCondition.h"
#include "Conductor/ConductorIdComponent.h"
#include "Conductor/Data/ContentConductorRow.h"
#include "Conductor/Data/ContentConductorPhaseRow.h"

#include "EngineUtils.h"

void UContentConductor::StartConductor(FName InContentId, const FContentConductorRow& Row)
{
	ContentId = InContentId;

	PhaseTable = Row.PhaseTable.LoadSynchronous();
	ActorTable = Row.ActorTable.LoadSynchronous();

	ValidateTables(Row.InitialPhase);

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

	ClearConditions();

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		if (!CurrentPhase.IsNone())
		{
			Module->ExitPhase(CurrentPhase);
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

	// 誤差の累積防止で超過分は次サイクルへ繰り越す
	EvaluateAccumulator = EvaluateInterval > 0.0f
							? FMath::Fmod(EvaluateAccumulator, EvaluateInterval)
							: 0.0f;

	EvaluateTransitions();
}

UContentConductorModule* UContentConductor::FindModuleByClass(
	TSubclassOf<UContentConductorModule> ModuleClass) const
{
	if (!ModuleClass) return nullptr;

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		if (Module && Module->IsA(ModuleClass))
		{
			return Module;
		}
	}

	return nullptr;
}

AActor* UContentConductor::FindManagedActor(FName ActorId)
{
	if (ActorId.IsNone()) return nullptr;

	if (const TWeakObjectPtr<AActor>* Spawned = SpawnedActors.Find(ActorId))
	{
		if (Spawned->IsValid()) return Spawned->Get();
	}

	return ResolvePlacedActor(ActorId);
}

TArray<AActor*> UContentConductor::GetGroupActors(FName GroupId)
{
	TArray<AActor*> Actors;
	if (!ActorTable || GroupId.IsNone()) return Actors;

	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentDirector::GetGroupActors"),
		[&](const FName& RowName, const FConductorActorRow& Row)
		{
			if (Row.GroupId != GroupId) return;

			if (AActor* Actor = FindManagedActor(RowName)) // ActorId = RowName
			{
				Actors.Add(Actor);
			}
		});

	return Actors;
}

void UContentConductor::EnterPhase(FName NewPhase)
{
	const FName OldPhase = CurrentPhase;

	// 1. 現フェーズの後始末
	ClearConditions();

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
	ApplyActorsForPhase(NewPhase);

	// 3. フェーズ開始のアクション実行 (現状同期前提)
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

	BuildConditions(*PhaseRow);

	EvaluateAccumulator = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[Conductor] %s: フェーズ %s -> %s"), *ContentId.ToString(), *OldPhase.ToString(), *NewPhase.ToString());

	OnPhaseChanged.Broadcast(this, OldPhase, NewPhase);
}

void UContentConductor::EvaluateTransitions()
{
	const FContentConductorPhaseRow* PhaseRow = FindPhaseRow(CurrentPhase);
	if (!PhaseRow) return;

	if (!ensureMsgf(PhaseConditions.Num() == PhaseRow->Transitions.Num(),
					TEXT("[Conductor] %s: フェーズ %s の条件数(%d)と遷移数(%d)が不一致"),
					*ContentId.ToString(),
					*CurrentPhase.ToString(),
					PhaseConditions.Num(),
					PhaseRow->Transitions.Num()))
	{
		return;
	}

	// 1. 決定のみ 配列順=優先度とする (最初にtrueになったConditionを採用)
	int32 TransitionIndex = INDEX_NONE;
	for (int32 Index = 0; Index < PhaseConditions.Num(); ++Index)
	{
		if (PhaseConditions[Index] && PhaseConditions[Index]->Evaluate())
		{
			TransitionIndex = Index;
			break;
		}
	}

	if (TransitionIndex == INDEX_NONE) return;

	// 2. ループを抜けてから適用
	EnterPhase(PhaseRow->Transitions[TransitionIndex].NextPhase);
}

void UContentConductor::BuildConditions(const FContentConductorPhaseRow& PhaseRow)
{
	ClearConditions();

	for (const FConductorPhaseTransition& Transition : PhaseRow.Transitions)
	{
		// 添字を合わせたいので詰める
		UConductorCondition* Condition = nullptr;
		if (Transition.Condition)
		{
			Condition = NewObject<UConductorCondition>(this, Transition.Condition);
		}

		PhaseConditions.Add(Condition);

		if (Condition)
		{
			Condition->BeginEvaluation(this);
		}
	}
}

void UContentConductor::ClearConditions()
{
	for (const TObjectPtr<UConductorCondition>& Condition : PhaseConditions)
	{
		if (Condition)
		{
			Condition->EndEvaluation();
		}
	}

	PhaseConditions.Reset();
}

const FContentConductorPhaseRow* UContentConductor::FindPhaseRow(FName Phase) const
{
	if (!PhaseTable) return nullptr;

	return PhaseTable->FindRow<FContentConductorPhaseRow>(
		Phase, TEXT("UContentConductor::FindPhaseRow"), false);
}

void UContentConductor::ValidateTables(FName InitialPhaseName) const
{
	// todo: ここで警告かエラー出したい
}

void UContentConductor::ApplyActorsForPhase(FName Phase)
{
	if (!ActorTable) return;

	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentDirector::ApplyActorsForPhase"),
		[&](const FName& RowName, const FConductorActorRow& Row)
		{
			const FConductorActorPhaseEntry* Entry = Row.Phases.FindByPredicate(
				[&](const FConductorActorPhaseEntry& Entry)
				{
					return Entry.Phase == Phase;
				});

			if (!Entry) return;

			ApplyState(RowName, Entry->State); // RowName = ActorId
		});
}

void UContentConductor::ApplyState(FName ActorId, EConductorActorState State)
{
	if (State == EConductorActorState::Removed)
	{
		//DestroySpawned(ActorId);

		if (AActor* Placed = ResolvePlacedActor(ActorId))
		{
			Placed->SetActorHiddenInGame(true);
			Placed->SetActorEnableCollision(false);
			Placed->SetActorTickEnabled(false);
		}

		return;
	}

	AActor* Actor = ResolvePlacedActor(ActorId);
	if (!Actor) return;

	const bool bActive = State == EConductorActorState::Active;

	Actor->SetActorHiddenInGame(State == EConductorActorState::Hidden);
	Actor->SetActorEnableCollision(bActive);
	Actor->SetActorTickEnabled(bActive);
}

AActor* UContentConductor::ResolvePlacedActor(FName ActorId)
{
	if (ActorId.IsNone()) return nullptr;

	if (const TWeakObjectPtr<AActor>* Cached = PlacedActors.Find(ActorId))
	{
		if (Cached->IsValid()) return Cached->Get();
	}

	// 一応サブレベルのストリームインで後から現れる場合があるので
	ScanPlacedActors();

	const TWeakObjectPtr<AActor>* Found = PlacedActors.Find(ActorId);

	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

void UContentConductor::ScanPlacedActors()
{
	UWorld* World = GetWorld();
	if (!World) return;

	PlacedActors.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UConductorIdComponent* IdComponent = It->FindComponentByClass<UConductorIdComponent>();
		if (!IdComponent || IdComponent->ActorId.IsNone()) continue;

		PlacedActors.Add(IdComponent->ActorId, *It);
	}
}
