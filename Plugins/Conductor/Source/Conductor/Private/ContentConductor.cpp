// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentConductor.h"

#include "ConductorModule.h"
#include "ConductorIdComponent.h"
#include "Data/ContentConductorRow.h"
#include "StateTree/ConductorStateTreeSchema.h"
#include "StateTree/ConductorStateTreeTasks.h"
#include "ConductorLog.h"

#include "EngineUtils.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"

void UContentConductor::StartConductor(FName InContentId, const FContentConductorRow& Row)
{
	ContentId	   = InContentId;
	StateTreeAsset = Row.StateTree.LoadSynchronous();
	ActorTable	   = Row.ActorTable.LoadSynchronous();

	ValidateData();

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

	StartStateTree();
}

void UContentConductor::StopConductor()
{
	if (!bStarted) return;

	StopStateTree();

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

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->TickModule(DeltaSeconds);
	}

	TickStateTree(DeltaSeconds);
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

	EnsureActorsScanned();

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
		TEXT("UContentConductor::GetGroupActors"),
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
	EnsureActorsScanned();

	const FName OldPhase = CurrentPhase.IsNone() ? ExitedPhase : CurrentPhase;

	CurrentPhase = NewPhase;
	ExitedPhase	 = NAME_None;

	ApplyActorsForPhase(NewPhase);

	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->EnterPhase(NewPhase);
	}

	UE_LOG(LogConductor, Log, TEXT("[Conductor] %s: フェーズ %s -> %s"), *ContentId.ToString(), *OldPhase.ToString(), *NewPhase.ToString());

	OnPhaseChanged.Broadcast(this, OldPhase, NewPhase);
}

void UContentConductor::ExitPhase(FName Phase)
{
	for (const TObjectPtr<UContentConductorModule>& Module : Modules)
	{
		Module->ExitPhase(Phase);
	}

	if (CurrentPhase == Phase)
	{
		ExitedPhase	 = Phase;
		CurrentPhase = NAME_None;
	}
}

void UContentConductor::SendStateTreeEvent(FGameplayTag Tag)
{
	if (!StateTreeAsset || !bStarted) return;

	FStateTreeMinimalExecutionContext Context(this, StateTreeAsset, StateTreeInstanceData);
	Context.SendEvent(Tag);
}

void UContentConductor::StartStateTree()
{
	if (!StateTreeAsset) return;

	FStateTreeExecutionContext Context(*this, *StateTreeAsset, StateTreeInstanceData);
	if (!SetStateTreeContext(Context)) return;

	Context.Start();
}

void UContentConductor::StopStateTree()
{
	if (!StateTreeAsset) return;

	FStateTreeExecutionContext Context(*this, *StateTreeAsset, StateTreeInstanceData);
	if (!SetStateTreeContext(Context)) return;

	Context.Stop();
}

void UContentConductor::TickStateTree(float DeltaSeconds)
{
	if (!StateTreeAsset) return;

	FStateTreeExecutionContext Context(*this, *StateTreeAsset, StateTreeInstanceData);
	if (!SetStateTreeContext(Context)) return;

	Context.Tick(DeltaSeconds);
}

bool UContentConductor::SetStateTreeContext(FStateTreeExecutionContext& Context)
{
	return UConductorStateTreeSchema::SetContextRequirements(*this, Context, true);
}

TSet<FName> UContentConductor::CollectTreePhases() const
{
	TSet<FName> Phases;
	if (!StateTreeAsset) return Phases;

	// フェーズ名はステート名ではなく「フェーズを適用」タスクのパラメータなので、
	// コンパイル済みツリーの既定インスタンスデータから拾う
	const FStateTreeInstanceData& Default = StateTreeAsset->GetDefaultInstanceData();
	for (int32 Index = 0; Index < Default.Num(); ++Index)
	{
		const FConductorTask_ApplyPhaseInstanceData* Data =
			Default.GetStruct(Index).GetPtr<const FConductorTask_ApplyPhaseInstanceData>();
		if (!Data || Data->Phase.IsNone()) continue;

		Phases.Add(Data->Phase);
	}

	return Phases;
}

void UContentConductor::ValidateData() const
{
	const FString Content = ContentId.ToString();

	if (!StateTreeAsset)
	{
		UE_SCREEN_LOG_ERROR(this, TEXT("%s: フェーズ定義(StateTree)が未設定"), *Content);
	}

	if (!ActorTable) return;

	const TSet<FName> TreePhases = CollectTreePhases();

	// 取れない場合は突き合わせを飛ばす (タスク未配置か、ツリーの内部表現が変わった場合)
	if (StateTreeAsset && TreePhases.IsEmpty())
	{
		UE_SCREEN_LOG_WARNING(this, TEXT("%s: ツリーからフェーズ名を取得できない (「フェーズを適用」タスクが置かれているか確認)"), *Content);
	}

	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentConductor::ValidateData"),
		[&](const FName& RowName, const FConductorActorRow& Row)
		{
			const FString Actor = RowName.ToString();

			if (Row.SpawnClass && Row.SpawnPointId.IsNone())
			{
				UE_SCREEN_LOG_WARNING(this, TEXT("%s/%s: 生成位置が未設定 (原点に出る)"), *Content, *Actor);
			}

			if (!Row.SpawnClass && !Row.SpawnPointId.IsNone())
			{
				UE_SCREEN_LOG_WARNING(this, TEXT("%s/%s: 配置アクターなのに生成位置が設定されている"), *Content, *Actor);
			}

			TSet<FName> Seen;
			for (const FConductorActorPhaseEntry& Entry : Row.Phases)
			{
				if (!TreePhases.IsEmpty() && !TreePhases.Contains(Entry.Phase))
				{
					UE_SCREEN_LOG_WARNING(this, TEXT("%s/%s: フェーズ %s がツリーに無い (適用されない)"), *Content, *Actor, *Entry.Phase.ToString());
				}

				bool bAlready = false;
				Seen.Add(Entry.Phase, &bAlready);
				if (!bAlready) continue;

				UE_SCREEN_LOG_WARNING(this, TEXT("%s/%s: フェーズ %s の指定が重複 (先頭のみ有効)"), *Content, *Actor, *Entry.Phase.ToString());
			}
		});
}

void UContentConductor::ApplyActorsForPhase(FName Phase)
{
	if (!ActorTable) return;

	ActorTable->ForeachRow<FConductorActorRow>(
		TEXT("UContentConductor::ApplyActorsForPhase"),
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

void UContentConductor::EnsureActorsScanned()
{
	if (bActorsScanned) return;

	bActorsScanned = true;

	ScanPlacedActors();
	VerifyPlacedActors();
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
