// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConductorObjectBase.h"
#include "Data/ConductorActorRow.h"
#include "ContentConductor.generated.h"

class UContentConductorModule;
class UConductorCondition;
class UContentConductorPhaseSet;
struct FContentConductorRow;
struct FContentConductorPhase;

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
	void RequestPhase(FName NextPhase);

	UFUNCTION(BlueprintCallable)
	UContentConductorModule* FindModuleByClass(
		TSubclassOf<UContentConductorModule> ModuleClass) const;

	template<typename T>
	T* FindModule() const
	{
		return Cast<T>(FindModuleByClass(T::StaticClass()));
	}

	UFUNCTION(BlueprintCallable)
	AActor* FindManagedActor(FName ActorId);

	UFUNCTION(BlueprintCallable)
	TArray<AActor*> GetGroupActors(FName GroupId);

	// フェーズ変更の際
	FOnContentPhaseChanged OnPhaseChanged;

private:
	void BeginContent();

	void EnterPhase(FName NewPhase);
	void EvaluateTransitions();

	void BuildConditions(const FContentConductorPhase& PhaseDef);
	void ClearConditions();
	const FContentConductorPhase* FindPhase(FName Phase) const;

	void ValidateData() const;

	// 開始時に集めたものが最後まで居る前提 対象アクターは bIsSpatiallyLoaded = false にすること
	void ScanPlacedActors();
	void VerifyPlacedActors() const;

	void ApplyActorsForPhase(FName Phase);
	void ApplyState(FName ActorId, const FConductorActorRow& Row, EConductorActorState State);
	AActor* ResolvePlacedActor(FName ActorId);

	AActor* EnsureSpawned(FName ActorId, const FConductorActorRow& Row);
	void DestroySpawned(FName ActorId);
	void DestroyAllSpawned();

	UPROPERTY()
	TObjectPtr<UContentConductorPhaseSet> PhaseSet;

	UPROPERTY()
	TObjectPtr<UDataTable> ActorTable;

	UPROPERTY()
	TArray<TSubclassOf<UContentConductorModule>> ModuleClasses;

	UPROPERTY()
	TArray<TObjectPtr<UContentConductorModule>> Modules;

	UPROPERTY()
	TObjectPtr<UConductorCondition> StartCondition;

	UPROPERTY()
	TArray<TObjectPtr<UConductorCondition>> PhaseConditions;

	// コンテンツに関連するレベル配置アクター
	TMap<FName, TWeakObjectPtr<AActor>> PlacedActors;
	// コンテンツが生成したアクター
	TMap<FName, TWeakObjectPtr<AActor>> SpawnedActors;

	FName ContentId;
	FName CurrentPhase;
	FName PendingPhase;

	bool bStarted		 = false;
	bool bContentStarted = false;
	bool bPhasePending	 = false;
};
