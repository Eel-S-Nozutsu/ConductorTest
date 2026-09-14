// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentConductor.h"

#include "Conductor/ConductorModule.h"
#include "Conductor/ConductorPhaseAction.h"
#include "Conductor/ConductorCondition.h"
#include "Conductor/ConductorIdComponent.h"
#include "Conductor/Data/ContentConductorRow.h"
#include "Conductor/Data/ContentConductorPhaseSet.h"

#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"

// 後でLog.hとかに移動
#define UE_SCREEN_LOG_ERROR(WorldContextObject, Format, ...)                                                          \
	{                                                                                                                 \
		FString ErrorMessage = FString::Printf(Format, ##__VA_ARGS__);                                                \
		UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMessage);                                                            \
		if (GEngine)                                                                                                  \
		{                                                                                                             \
			UKismetSystemLibrary::PrintString(WorldContextObject, ErrorMessage, true, true, FLinearColor::Red, 2.0f); \
		}                                                                                                             \
	}

void UContentConductor::StartConductor(FName InContentId, const FContentConductorRow& Row)
{
	ContentId	  = InContentId;
	PhaseSet	  = Row.PhaseSet.LoadSynchronous();
	ActorTable	  = Row.ActorTable.LoadSynchronous();
	ModuleClasses = Row.Modules;

	ValidateData();

	bStarted = true;

	if (PhaseSet && PhaseSet->StartCondition)
	{
		StartCondition = DuplicateObject<UConductorCondition>(PhaseSet->StartCondition, this);
		StartCondition->BeginEvaluation(this);

		return;
	}

	BeginContent();
}

void UContentConductor::BeginContent()
{
	if (StartCondition)
	{
		StartCondition->EndEvaluation();
		StartCondition = nullptr;
	}

	// 以降は再走査しないので、ここで揃っていないとコンテンツ中は欠けたままになる
	ScanPlacedActors();
	VerifyPlacedActors();

	for (const TSubclassOf<UContentConductorModule>& ModuleClass : ModuleClasses)
	{
		if (!ModuleClass) continue;

		UContentConductorModule* Module = NewObject<UContentConductorModule>(this, ModuleClass);
		Modules.Add(Module);
	}

	bContentStarted = true;

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->StartModule();
	}

	EnterPhase(PhaseSet ? PhaseSet->InitialPhase : NAME_None);
}

void UContentConductor::StopConductor()
{
	if (!bStarted) return;

	if (StartCondition)
	{
		StartCondition->EndEvaluation();
		StartCondition = nullptr;
	}

	if (bContentStarted)
	{
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
	}

	bContentStarted = false;
	bStarted		= false;
}

void UContentConductor::TickConductor(float DeltaSeconds)
{
	if (!bStarted) return;

	if (!bContentStarted)
	{
		if (StartCondition && StartCondition->Evaluate())
		{
			BeginContent();
		}

		return;
	}

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
	if (!bContentStarted || NextPhase.IsNone()) return;

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
		UE_SCREEN_LOG_ERROR(this, TEXT("[Conductor] %s: 遷移先のフェーズが未設定"), *ContentId.ToString());
		return;
	}

	const FContentConductorPhase* PhaseDef = FindPhase(NewPhase);
	if (!PhaseDef)
	{
		UE_SCREEN_LOG_ERROR(this, TEXT("[Conductor] %s: フェーズ %s が DataAsset に無い"), *ContentId.ToString(), *NewPhase.ToString());
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

void UContentConductor::ValidateData() const
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

	const TWeakObjectPtr<AActor>* Cached = PlacedActors.Find(ActorId);

	return Cached && Cached->IsValid() ? Cached->Get() : nullptr;
}

void UContentConductor::ScanPlacedActors()
{
	PlacedActors.Reset();

	UWorld* World = GetWorld();
	if (!World || !ActorTable) return;

	TSet<FName> TargetIds;
	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentConductor::ScanPlacedActors"),
		[&](const FName& RowName, const FConductorActorRow& Row)
		{
			if (!Row.SpawnClass)
			{
				TargetIds.Add(RowName); // RowName = ActorId
			}

			if (!Row.SpawnPointId.IsNone())
			{
				TargetIds.Add(Row.SpawnPointId);
			}
		});

	if (TargetIds.IsEmpty()) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UConductorIdComponent* IdComponent = It->FindComponentByClass<UConductorIdComponent>();
		if (!IdComponent || !TargetIds.Contains(IdComponent->ActorId)) continue;

		PlacedActors.Add(IdComponent->ActorId, *It);
	}
}

void UContentConductor::VerifyPlacedActors() const
{
	if (!ActorTable) return;

	TArray<FName> Missing;
	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentConductor::VerifyPlacedActors"),
		[&](const FName& RowName, const FConductorActorRow& Row)
		{
			if (!Row.SpawnClass && !PlacedActors.Contains(RowName))
			{
				Missing.AddUnique(RowName);
			}

			if (!Row.SpawnPointId.IsNone() && !PlacedActors.Contains(Row.SpawnPointId))
			{
				Missing.AddUnique(Row.SpawnPointId);
			}
		});

	if (Missing.IsEmpty()) return;

	const FString Ids = FString::JoinBy(
		Missing, TEXT(", "), [](const FName& Id)
		{
			return Id.ToString();
		});

	UE_SCREEN_LOG_ERROR(this, TEXT("[Conductor] %s: 配置アクターが %d 件見つからない (%s)"), *ContentId.ToString(), Missing.Num(), *Ids);
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
			UE_SCREEN_LOG_ERROR(this, TEXT("[Conductor] %s/%s: 生成位置(%s)が見つからないので生成しない"), *ContentId.ToString(), *ActorId.ToString(), *Row.SpawnPointId.ToString());
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
