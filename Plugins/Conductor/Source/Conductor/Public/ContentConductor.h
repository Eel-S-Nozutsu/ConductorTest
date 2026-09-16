// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConductorObjectBase.h"
#include "Data/ConductorActorRow.h"
#include "GameplayTagContainer.h"
#include "StateTreeInstanceData.h"
#include "ContentConductor.generated.h"

class UContentConductorModule;
class UStateTree;
struct FContentConductorRow;
struct FStateTreeExecutionContext;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContentPhaseChanged, UContentConductor*, FName, FName);

/**
 * 1コンテンツの進行役
 */
UCLASS(BlueprintType)
class CONDUCTOR_API UContentConductor : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	void StartConductor(FName InContentId, const FContentConductorRow& Row);
	void StopConductor();
	void TickConductor(float DeltaSeconds);

	UFUNCTION(BlueprintPure)
	FName GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure)
	FName GetContentId() const { return ContentId; }

	UFUNCTION(BlueprintCallable)
	UContentConductorModule* FindModuleByClass(
		TSubclassOf<UContentConductorModule> ModuleClass) const;

	template<typename T>
	T* FindModule() const
	{
		return Cast<T>(FindModuleByClass(T::StaticClass()));
	}

	// ActorIdに対応するアクターを返す
	UFUNCTION(BlueprintCallable)
	AActor* FindManagedActor(FName ActorId);

	// GroupIdに所属するグループのアクターを返す
	UFUNCTION(BlueprintCallable)
	TArray<AActor*> GetGroupActors(FName GroupId);

	// フェーズ開始 ツリーのタスクから呼ばれる
	void EnterPhase(FName NewPhase);
	// フェーズ終了 ツリーのタスクから呼ばれる
	void ExitPhase(FName Phase);

	// ツリーにイベントを送る
	UFUNCTION(BlueprintCallable)
	void SendStateTreeEvent(FGameplayTag Tag);

	// フェーズ変更の際
	FOnContentPhaseChanged OnPhaseChanged;

private:
	TSet<FName> CollectTreePhases() const;
	void ValidateData() const;

	void StartStateTree();
	void StopStateTree();
	void TickStateTree(float DeltaSeconds);
	bool SetStateTreeContext(FStateTreeExecutionContext& Context);

	void EnsureActorsScanned();
	void ScanPlacedActors();
	void VerifyPlacedActors() const;

	void ApplyActorsForPhase(FName Phase);
	void ApplyState(FName ActorId, const FConductorActorRow& Row, EConductorActorState State);
	AActor* ResolvePlacedActor(FName ActorId);

	AActor* EnsureSpawned(FName ActorId, const FConductorActorRow& Row);
	void DestroySpawned(FName ActorId);
	void DestroyAllSpawned();

	UPROPERTY()
	TObjectPtr<UStateTree> StateTreeAsset;

	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

	UPROPERTY()
	TObjectPtr<UDataTable> ActorTable;

	UPROPERTY()
	TArray<TObjectPtr<UContentConductorModule>> Modules;

	// コンテンツに関連するレベル配置アクター
	TMap<FName, TWeakObjectPtr<AActor>> PlacedActors;
	// コンテンツが生成したアクター
	TMap<FName, TWeakObjectPtr<AActor>> SpawnedActors;

	FName ContentId;
	FName CurrentPhase;

	// ツリーはExitStateを先に流すので 次のEnterまで直前のフェーズを保持
	FName ExitedPhase;

	bool bStarted		= false;
	bool bActorsScanned = false;
};
