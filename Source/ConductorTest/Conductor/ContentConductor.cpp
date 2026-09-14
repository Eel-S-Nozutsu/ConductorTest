// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentConductor.h"

#include "Conductor/ConductorModule.h"
#include "Conductor/ConductorPhaseAction.h"
#include "Conductor/ConductorCondition.h"
#include "Conductor/ConductorIdComponent.h"
#include "Conductor/Data/ContentConductorRow.h"
#include "Conductor/Data/ContentConductorPhaseSet.h"

#include "EngineUtils.h"

void UContentConductor::StartConductor(FName InContentId, const FContentConductorRow& Row)
{
	ContentId = InContentId;

	PhaseSet   = Row.PhaseSet.LoadSynchronous();
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

	DestroyAllSpawned();

	Modules.Reset();
	bStarted = false;
}

void UContentConductor::TickConductor(float DeltaSeconds)
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

	EvaluateTransitions();
}

void UContentConductor::RequestPhase(FName NextPhase)
{
	if (!bStarted || NextPhase.IsNone()) return;

	PendingPhase  = NextPhase;
	bPhasePending = true;
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

	const FContentConductorPhase* PhaseDef = FindPhase(NewPhase);
	if (!PhaseDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Conductor] %s: フェーズ %s が DataAsset に無い"), *ContentId.ToString(), *NewPhase.ToString());
		return;
	}

	// 2. アクターのポップ状態
	ApplyActorsForPhase(NewPhase);

	// 3. フェーズ開始のアクション実行 (現状同期前提)
	for (const TObjectPtr<UConductorPhaseAction>& ActionTemplate : PhaseDef->EntryActions)
	{
		if (!ActionTemplate) continue;

		UConductorPhaseAction* Action = DuplicateObject<UConductorPhaseAction>(ActionTemplate, this);
		Action->Execute(this);
	}

	// 4. Moduleへ通知
	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->EnterPhase(NewPhase);
	}

	BuildConditions(*PhaseDef);

	UE_LOG(LogTemp, Log, TEXT("[Conductor] %s: フェーズ %s -> %s"), *ContentId.ToString(), *OldPhase.ToString(), *NewPhase.ToString());

	OnPhaseChanged.Broadcast(this, OldPhase, NewPhase);
}

void UContentConductor::EvaluateTransitions()
{
	const FContentConductorPhase* PhaseDef = FindPhase(CurrentPhase);
	if (!PhaseDef) return;

	if (!ensureMsgf(PhaseConditions.Num() == PhaseDef->Transitions.Num(),
					TEXT("[Conductor] %s: フェーズ %s の条件数(%d)と遷移数(%d)が不一致"),
					*ContentId.ToString(),
					*CurrentPhase.ToString(),
					PhaseConditions.Num(),
					PhaseDef->Transitions.Num()))
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
	EnterPhase(PhaseDef->Transitions[TransitionIndex].NextPhase);
}

void UContentConductor::BuildConditions(const FContentConductorPhase& PhaseDef)
{
	ClearConditions();

	for (const FConductorPhaseTransition& Transition : PhaseDef.Transitions)
	{
		// 添字を合わせたいので詰める
		UConductorCondition* Condition = nullptr;
		if (Transition.Condition)
		{
			Condition = DuplicateObject<UConductorCondition>(Transition.Condition, this);
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

const FContentConductorPhase* UContentConductor::FindPhase(FName Phase) const
{
	if (!PhaseSet) return nullptr;

	return PhaseSet->Phases.Find(Phase);
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

			ApplyState(RowName, Row, Entry->State); // RowName = ActorId
		});
}

void UContentConductor::ApplyState(FName ActorId, const FConductorActorRow& Row, EConductorActorState State)
{
	if (State == EConductorActorState::Removed)
	{
		if (Row.SpawnClass)
		{
			DestroySpawned(ActorId);
			return;
		}

		if (AActor* Placed = ResolvePlacedActor(ActorId))
		{
			Placed->SetActorHiddenInGame(true);
			Placed->SetActorEnableCollision(false);
			Placed->SetActorTickEnabled(false);
		}

		return;
	}

	AActor* Actor = Row.SpawnClass ? EnsureSpawned(ActorId, Row) : ResolvePlacedActor(ActorId);
	if (!Actor) return;

	bool bHidden	= false;
	bool bCollision = true;
	bool bTick		= true;

	switch (State)
	{
	case EConductorActorState::Active:
		break;
	case EConductorActorState::Hidden:
		bHidden	   = true;
		bCollision = false;
		break;
	case EConductorActorState::Frozen:
		bTick = false;
		break;
	default:
		checkNoEntry();
		break;
	}

	Actor->SetActorHiddenInGame(bHidden);
	Actor->SetActorEnableCollision(bCollision);
	Actor->SetActorTickEnabled(bTick);
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

AActor* UContentConductor::EnsureSpawned(FName ActorId, const FConductorActorRow& Row)
{
	if (const TWeakObjectPtr<AActor>* Existing = SpawnedActors.Find(ActorId))
	{
		if (Existing->IsValid()) return Existing->Get();
	}

	UWorld* World = GetWorld();
	if (!World || !Row.SpawnClass) return nullptr;

	FTransform SpawnTransform = FTransform::Identity;

	if (!Row.SpawnPointId.IsNone())
	{
		const AActor* SpawnPoint = ResolvePlacedActor(Row.SpawnPointId);
		if (!SpawnPoint)
		{
			UE_LOG(LogTemp, Error, TEXT("[Conductor] %s/%s: 生成位置(%s)が見つからないので生成しない"), *ContentId.ToString(), *ActorId.ToString(), *Row.SpawnPointId.ToString());
			return nullptr;
		}

		SpawnTransform = SpawnPoint->GetActorTransform();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(Row.SpawnClass, SpawnTransform, Params);
	if (!Spawned) return nullptr;

	SpawnedActors.Add(ActorId, Spawned);

	return Spawned;
}

void UContentConductor::DestroySpawned(FName ActorId)
{
	TWeakObjectPtr<AActor> Spawned;
	if (!SpawnedActors.RemoveAndCopyValue(ActorId, Spawned)) return;

	if (Spawned.IsValid())
	{
		Spawned->Destroy();
	}
}

void UContentConductor::DestroyAllSpawned()
{
	for (const TPair<FName, TWeakObjectPtr<AActor>>& Pair : SpawnedActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}

	SpawnedActors.Reset();
}
