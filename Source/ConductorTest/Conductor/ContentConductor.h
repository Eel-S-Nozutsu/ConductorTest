// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conductor/ConductorObjectBase.h"
#include "ContentConductor.generated.h"

class UContentConductorModule;
struct FContentConductorRow;
struct FContentConductorPhaseRow;

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

private:
	void EnterPhase(FName NewPhase);
	void EvaluateTransitions();
	void ClearTransitions();

	const FContentConductorPhaseRow* FindPhaseRow(FName Phase) const;

	UPROPERTY()
	TArray<TObjectPtr<UContentConductorModule>> Modules;

	UPROPERTY()
	TObjectPtr<UDataTable> PhaseTable;

	FName ContentId;
	FName CurrentPhase;
	FName PendingPhase;

	float EvaluateAccumulator = 0.0f;

	bool bStarted	   = false;
	bool bPhasePending = false;
};
