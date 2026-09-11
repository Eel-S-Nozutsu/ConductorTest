// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContentDirector.h"

#include "EngineUtils.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/Director/DirectorIdComponent.h"
#include "PRJ_TIDE_P0/Data/Director/ContentDirectorRow.h"
#include "PRJ_TIDE_P0/Data/Director/DirectorActorRow.h"
#include "PRJ_TIDE_P0/Director/DirectorCondition.h"
#include "PRJ_TIDE_P0/Director/DirectorModule.h"
#include "PRJ_TIDE_P0/Director/DirectorPhaseAction.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGimmickResettable.h"
#include "PRJ_TIDE_P0/PRJ_TIDE_P0.h"

void UContentDirector::StartDirector(FName InContentId, const FContentDirectorRow& Row)
{
	ContentId = InContentId;

	PhaseTable      = Row.PhaseTable.LoadSynchronous();
	ActorTable      = Row.ActorTable.LoadSynchronous();
	ActorPhaseTable = Row.ActorPhaseTable.LoadSynchronous();

	ValidateTables(Row.InitialPhase);

	for (const TSubclassOf<UContentDirectorModule>& ModuleClass : Row.Modules)
	{
		if (!ModuleClass) continue;

		UContentDirectorModule* Module = NewObject<UContentDirectorModule>(this, ModuleClass);
		Modules.Add(Module);
	}

	bStarted = true;

	for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
	{
		Module->StartModule();
	}

	EnterPhase(Row.InitialPhase);
}

void UContentDirector::StopDirector()
{
	if (!bStarted) return;

	ClearConditions();

	for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
	{
		if (CurrentPhase.IsValid())
		{
			Module->ExitPhase(CurrentPhase);
		}
		Module->StopModule();
	}

	DestroyAllSpawned();

	Modules.Reset();
	bStarted = false;
}

void UContentDirector::TickDirector(float DeltaSeconds, float EvaluateInterval)
{
	if (!bStarted) return;

	// 明示要求は条件評価より先に処理する
	if (bPhasePending)
	{
		bPhasePending = false;
		EnterPhase(PendingPhase);
	}

	for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
	{
		Module->TickModule(DeltaSeconds);
	}

	EvaluateAccumulator += DeltaSeconds;
	if (EvaluateAccumulator < EvaluateInterval) return;

	EvaluateAccumulator = 0.0f;
	EvaluateTransitions();
}

void UContentDirector::RequestPhase(FName NextPhase)
{
	if (NextPhase.IsNone()) return;

	PendingPhase  = NextPhase;
	bPhasePending = true;
}

void UContentDirector::RestartPhase()
{
	RequestPhase(CurrentPhase);
}

UContentDirectorModule* UContentDirector::FindModuleByClass(TSubclassOf<UContentDirectorModule> ModuleClass) const
{
	if (!ModuleClass) return nullptr;

	for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
	{
		if (Module && Module->IsA(ModuleClass))
		{
			return Module;
		}
	}

	return nullptr;
}

AActor* UContentDirector::FindManagedActor(FName ActorId)
{
	if (ActorId.IsNone()) return nullptr;

	if (const TWeakObjectPtr<AActor>* Spawned = SpawnedActors.Find(ActorId))
	{
		if (Spawned->IsValid()) return Spawned->Get();
	}

	return ResolvePlacedActor(ActorId);
}

TArray<AActor*> UContentDirector::GetGroupActors(FName GroupId)
{
	TArray<AActor*> Actors;

	ForEachActorRow([&](FName ActorId, const FDirectorActorRow& Row)
	{
		if (GroupId.IsNone() || Row.GroupId != GroupId) return;

		if (AActor* Actor = FindManagedActor(ActorId))
		{
			Actors.Add(Actor);
		}
	});

	return Actors;
}

int32 UContentDirector::CountAliveInGroup(FName GroupId)
{
	int32 AliveCount = 0;

	for (AActor* Actor : GetGroupActors(GroupId))
	{
		const UStatusComponent* Status = Actor->FindComponentByClass<UStatusComponent>();
		if (!Status || !Status->IsDead())
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

TArray<FName> UContentDirector::GetDefinedPhases() const
{
	return PhaseTable ? PhaseTable->GetRowNames() : TArray<FName>();
}

void UContentDirector::GetActiveTransitionsDebug(TArray<TTuple<FString, FName, bool>>& OutRows) const
{
	const FContentDirectorPhaseRow* PhaseRow = FindPhaseRow(CurrentPhase);
	if (!PhaseRow) return;

	for (int32 Index = 0; Index < PhaseRow->Transitions.Num(); ++Index)
	{
		UDirectorCondition* Condition = PhaseConditions.IsValidIndex(Index) ? PhaseConditions[Index].Get() : nullptr;
		if (!Condition) continue;

		OutRows.Emplace(Condition->GetDebugText(), PhaseRow->Transitions[Index].NextPhase, Condition->Evaluate());
	}
}

void UContentDirector::EnterPhase(FName NewPhase)
{
	const FName OldPhase = CurrentPhase;

	// 1. 現フェーズの後始末
	ClearConditions();

	if (!OldPhase.IsNone())
	{
		for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
		{
			Module->ExitPhase(OldPhase);
		}
	}

	CurrentPhase = NewPhase;

	if (NewPhase.IsNone())
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: 遷移先のフェーズが未設定"), *ContentId.ToString());
		return;
	}

	const FContentDirectorPhaseRow* PhaseRow = FindPhaseRow(NewPhase);
	if (!PhaseRow)
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: フェーズ %s の行がDTに無い"),
			*ContentId.ToString(), *NewPhase.ToString());
		return;
	}

	// 2. アクターのポップ状態
	ApplyActorsForPhase(NewPhase);

	// 3. フェーズ開始時のアクション
	for (const TSubclassOf<UDirectorPhaseAction>& ActionClass : PhaseRow->EntryActions)
	{
		if (!ActionClass) continue;

		UDirectorPhaseAction* Action = NewObject<UDirectorPhaseAction>(this, ActionClass);
		Action->Execute(this);
	}

	// 4. Moduleへ通知
	for (const TObjectPtr<UContentDirectorModule>& Module : Modules)
	{
		Module->EnterPhase(NewPhase);
	}

	// 出口の条件はここで有効化する。入場処理の副作用で即遷移しないよう最後に回す
	BuildConditions(*PhaseRow);

	EvaluateAccumulator = 0.0f;

	UE_LOG(LogPRJ_TIDE_P0, Log, TEXT("[Director] %s: フェーズ %s -> %s"),
		*ContentId.ToString(), *OldPhase.ToString(), *NewPhase.ToString());

	OnPhaseChanged.Broadcast(this, OldPhase, NewPhase);
}

void UContentDirector::EvaluateTransitions()
{
	const FContentDirectorPhaseRow* PhaseRow = FindPhaseRow(CurrentPhase);
	if (!PhaseRow) return;

	// 実行中にDTを編集された場合。条件だけ作り直す(アクターには触らない)
	if (PhaseConditions.Num() != PhaseRow->Transitions.Num())
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s/%s: 出口の数がDTと合わないので条件を作り直す"),
			*ContentId.ToString(), *CurrentPhase.ToString());

		BuildConditions(*PhaseRow);
		return;
	}

	// 上から最初に成立した辺を採ってそこで打ち切る
	for (int32 Index = 0; Index < PhaseConditions.Num(); ++Index)
	{
		if (!PhaseConditions[Index] || !PhaseConditions[Index]->Evaluate()) continue;

		// EnterPhaseがPhaseConditionsを作り直すので、遷移先は先に控える
		const FName NextPhase = PhaseRow->Transitions[Index].NextPhase;

		EnterPhase(NextPhase);
		return;
	}
}

void UContentDirector::BuildConditions(const FContentDirectorPhaseRow& PhaseRow)
{
	ClearConditions();

	for (const FDirectorPhaseTransition& Transition : PhaseRow.Transitions)
	{
		// 条件未設定の出口も添字を合わせるために詰める
		UDirectorCondition* Condition = Transition.Condition
			? NewObject<UDirectorCondition>(this, Transition.Condition)
			: nullptr;

		PhaseConditions.Add(Condition);

		if (Condition)
		{
			Condition->BeginEvaluation(this);
		}
	}
}

void UContentDirector::ClearConditions()
{
	for (const TObjectPtr<UDirectorCondition>& Condition : PhaseConditions)
	{
		if (Condition)
		{
			Condition->EndEvaluation();
		}
	}

	PhaseConditions.Reset();
}

const FContentDirectorPhaseRow* UContentDirector::FindPhaseRow(FName Phase) const
{
	if (!PhaseTable) return nullptr;

	return PhaseTable->FindRow<FContentDirectorPhaseRow>(Phase, TEXT("UContentDirector::FindPhaseRow"), false);
}

void UContentDirector::ValidateTables(FName InitialPhaseName) const
{
	const FString Id = ContentId.ToString();

	if (!PhaseTable)
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: フェーズ表が未設定"), *Id);
		return;
	}

	if (!FindPhaseRow(InitialPhaseName))
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: 初期フェーズ %s の行が無い"),
			*Id, *InitialPhaseName.ToString());
	}

	// 遷移先が自分のフェーズ表の中に在るか
	PhaseTable->ForeachRow<FContentDirectorPhaseRow>(TEXT("UContentDirector::ValidateTables"),
		[&](const FName& PhaseName, const FContentDirectorPhaseRow& PhaseRow)
		{
			for (const FDirectorPhaseTransition& Transition : PhaseRow.Transitions)
			{
				if (!Transition.Condition)
				{
					UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s/%s: 条件が未設定の出口がある"),
						*Id, *PhaseName.ToString());
				}

				if (!FindPhaseRow(Transition.NextPhase))
				{
					UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s/%s: 遷移先 %s の行が無い"),
						*Id, *PhaseName.ToString(), *Transition.NextPhase.ToString());
				}
			}
		});

	if (!ActorPhaseTable) return;

	// アクターの差分行が自分の台帳とフェーズ表を指しているか
	ActorPhaseTable->ForeachRow<FDirectorActorPhaseRow>(TEXT("UContentDirector::ValidateTables"),
		[&](const FName& RowName, const FDirectorActorPhaseRow& Row)
		{
			if (!FindPhaseRow(Row.Phase))
			{
				UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: 差分行 %s のフェーズ %s が無い"),
					*Id, *RowName.ToString(), *Row.Phase.ToString());
			}

			// 未設定はそのフェーズの既定行なので、指しているときだけ検査する
			if (Row.ActorId.IsNone()) return;

			if (!ActorTable
				|| !ActorTable->FindRow<FDirectorActorRow>(Row.ActorId, TEXT("UContentDirector::ValidateTables"), false))
			{
				UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s: 差分行 %s のアクター %s が台帳に無い"),
					*Id, *RowName.ToString(), *Row.ActorId.ToString());
			}
		});
}

void UContentDirector::ApplyActorsForPhase(FName Phase)
{
	ForEachActorRow([&](FName ActorId, const FDirectorActorRow& Row)
	{
		const FDirectorActorPhaseRow* PhaseRow = FindActorPhaseRow(ActorId, Phase);

		// 記述が無いアクターは既定(Active)で扱う。フェーズ数ぶん行を書かせないため
		const EDirectorActorPresence Presence = PhaseRow ? PhaseRow->Presence : EDirectorActorPresence::Active;
		const float HealthRatio               = PhaseRow ? PhaseRow->HealthRatio : 0.0f;

		ApplyPresence(ActorId, Row, Presence, HealthRatio);
	});
}

void UContentDirector::ApplyPresence(FName ActorId, const FDirectorActorRow& ActorRow, EDirectorActorPresence Presence, float HealthRatio)
{
	if (Presence == EDirectorActorPresence::Removed)
	{
		// 生成した実体は破棄。レベル配置アクターは隠すだけ(勝手に消すとやり直しで戻せない)
		DestroySpawned(ActorId);

		if (AActor* Placed = ResolvePlacedActor(ActorId))
		{
			Placed->SetActorHiddenInGame(true);
			Placed->SetActorEnableCollision(false);
			Placed->SetActorTickEnabled(false);
		}

		return;
	}

	AActor* Actor = ActorRow.SpawnClass ? EnsureSpawned(ActorId, ActorRow) : ResolvePlacedActor(ActorId);
	if (!Actor) return;

	const bool bActive = Presence == EDirectorActorPresence::Active;

	Actor->SetActorHiddenInGame(Presence == EDirectorActorPresence::Hidden);
	Actor->SetActorEnableCollision(bActive);
	Actor->SetActorTickEnabled(bActive);

	if (!bActive) return;

	// やり直しで初期状態へ戻す。何が初期状態かはギミック側の都合なので通知だけ渡す
	if (Actor->Implements<UGimmickResettable>())
	{
		IGimmickResettable::Execute_ResetGimmick(Actor);
	}

	if (HealthRatio <= 0.0f) return;

	if (UStatusComponent* Status = Actor->FindComponentByClass<UStatusComponent>())
	{
		const float TargetHP = Status->GetMaxHP() * FMath::Clamp(HealthRatio, 0.0f, 1.0f);

		if (Status->IsDead())
		{
			Status->Revive(HealthRatio);
		}
		else
		{
			Status->ModifyHP(TargetHP - Status->GetCurrentHP());
		}
	}
}

const FDirectorActorPhaseRow* UContentDirector::FindActorPhaseRow(FName ActorId, FName Phase) const
{
	if (!ActorPhaseTable) return nullptr;

	const FDirectorActorPhaseRow* Found   = nullptr;
	const FDirectorActorPhaseRow* Default = nullptr;

	ActorPhaseTable->ForeachRow<FDirectorActorPhaseRow>(TEXT("UContentDirector::FindActorPhaseRow"),
		[&](const FName&, const FDirectorActorPhaseRow& Row)
		{
			if (Row.Phase != Phase) return;

			if (!Found && Row.ActorId == ActorId)
			{
				Found = &Row;
			}
			// 対象アクター未指定の行はそのフェーズの既定として使う
			else if (!Default && Row.ActorId.IsNone())
			{
				Default = &Row;
			}
		});

	return Found ? Found : Default;
}

void UContentDirector::ForEachActorRow(TFunctionRef<void(FName, const FDirectorActorRow&)> Visitor) const
{
	if (!ActorTable) return;

	ActorTable->ForeachRow<FDirectorActorRow>(TEXT("UContentDirector::ForEachActorRow"),
		[&](const FName& RowName, const FDirectorActorRow& Row)
		{
			Visitor(RowName, Row);
		});
}

AActor* UContentDirector::ResolvePlacedActor(FName ActorId)
{
	if (ActorId.IsNone()) return nullptr;

	if (const TWeakObjectPtr<AActor>* Cached = PlacedActors.Find(ActorId))
	{
		if (Cached->IsValid()) return Cached->Get();
	}

	// 見つからないうちは走査し直す。サブレベルのストリームインで後から現れる場合がある
	ScanPlacedActors();

	const TWeakObjectPtr<AActor>* Found = PlacedActors.Find(ActorId);

	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

void UContentDirector::ScanPlacedActors()
{
	UWorld* World = GetWorld();
	if (!World) return;

	PlacedActors.Reset();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UDirectorIdComponent* IdComponent = It->FindComponentByClass<UDirectorIdComponent>();
		if (!IdComponent || IdComponent->ActorId.IsNone()) continue;

		PlacedActors.Add(IdComponent->ActorId, *It);
	}
}

AActor* UContentDirector::EnsureSpawned(FName ActorId, const FDirectorActorRow& Row)
{
	if (const TWeakObjectPtr<AActor>* Existing = SpawnedActors.Find(ActorId))
	{
		if (Existing->IsValid()) return Existing->Get();
	}

	UWorld* World = GetWorld();
	if (!World || !Row.SpawnClass) return nullptr;

	FTransform SpawnTransform = FTransform::Identity;

	if (const AActor* SpawnPoint = ResolvePlacedActor(Row.SpawnPointId))
	{
		SpawnTransform = SpawnPoint->GetActorTransform();
	}
	else
	{
		UE_LOG(LogPRJ_TIDE_P0, Warning, TEXT("[Director] %s/%s: 生成位置(%s)が見つからないので原点に出す"),
			*ContentId.ToString(), *ActorId.ToString(), *Row.SpawnPointId.ToString());
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(Row.SpawnClass, SpawnTransform, Params);
	if (!Spawned) return nullptr;

	SpawnedActors.Add(ActorId, Spawned);

	return Spawned;
}

void UContentDirector::DestroySpawned(FName ActorId)
{
	TWeakObjectPtr<AActor> Spawned;
	if (!SpawnedActors.RemoveAndCopyValue(ActorId, Spawned)) return;

	if (Spawned.IsValid())
	{
		Spawned->Destroy();
	}
}

void UContentDirector::DestroyAllSpawned()
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
