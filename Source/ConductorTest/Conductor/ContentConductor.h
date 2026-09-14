// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conductor/ConductorObjectBase.h"
#include "Conductor/Data/ConductorActorRow.h"
#include "ContentConductor.generated.h"

class UContentConductorModule;
class UConductorCondition;
struct FContentConductorRow;
struct FContentConductorPhaseRow;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContentPhaseChanged, UContentConductor*, FName, FName);

/**
 * 1コンテンツの進行役
 */
UCLASS(Blueprintable, BlueprintType)
class CONDUCTORTEST_API UContentConductor : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	void StartConductor(FName InContentId, const FContentConductorRow& Row);
	void StopConductor();
	void TickConductor(float DeltaSeconds, float EvaluateInterval);

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

	UFUNCTION(BlueprintCallable)
	AActor* FindManagedActor(FName ActorId);

	UFUNCTION(BlueprintCallable)
	TArray<AActor*> GetGroupActors(FName GroupId);

	// フェーズ変更の際
	FOnContentPhaseChanged OnPhaseChanged;

private:
	void EnterPhase(FName NewPhase);
	void EvaluateTransitions();

	void BuildConditions(const FContentConductorPhaseRow& PhaseRow);
	void ClearConditions();
	const FContentConductorPhaseRow* FindPhaseRow(FName Phase) const;

	void ValidateTables(FName InitialPhaseName) const;

	void ApplyActorsForPhase(FName Phase);
	void ApplyState(FName ActorId, const FConductorActorRow& ActorRow, EConductorActorState State);
	AActor* ResolvePlacedActor(FName ActorId);
	void ScanPlacedActors();

	UPROPERTY()
	TObjectPtr<UDataTable> PhaseTable;

	UPROPERTY()
	TObjectPtr<UDataTable> ActorTable;

	UPROPERTY()
	TArray<TObjectPtr<UContentConductorModule>> Modules;

	UPROPERTY()
	TArray<TObjectPtr<UConductorCondition>> PhaseConditions;

	// コンテンツに関連するレベル配置アクター
	TMap<FName, TWeakObjectPtr<AActor>> PlacedActors;
	// コンテンツが生成したアクター
	TMap<FName, TWeakObjectPtr<AActor>> SpawnedActors;

	FName ContentId;
	FName CurrentPhase;
	FName PendingPhase;

	float EvaluateAccumulator = 0.0f;

	bool bStarted	   = false;
	bool bPhasePending = false;
};
