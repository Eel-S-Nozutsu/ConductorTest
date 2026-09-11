// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conductor/ConductorObjectBase.h"
#include "MapConductor.generated.h"

class UContentConductor;
class UMapConductorModule;
struct FMapConductorRow;

/**
 * 1レベルに1つ存在
 * 侵入時にそのレベルに属するContentConductorを生成する
 */
UCLASS(Blueprintable, BlueprintType)
class CONDUCTORTEST_API UMapConductor : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	void StartConductor(const FMapConductorRow& Row, FName InLevelPackageName);
	void StopConductor();
	void TickConductor(float DeltaSeconds, float EvaluateInterval);

	UFUNCTION(BlueprintCallable)
	UContentConductor* FindContentConductor(FName ContentId) const;

	FName GetLevelPackageName() const { return LevelPackageName; }

	const TArray<TObjectPtr<UContentConductor>>&
		GetContentConductors() const { return ContentConductors; }

private:
	UPROPERTY()
	TObjectPtr<UDataTable> ContentConductorList;

	UPROPERTY()
	TArray<TObjectPtr<UMapConductorModule>> Modules;

	UPROPERTY()
	TArray<TObjectPtr<UContentConductor>> ContentConductors;

	FName LevelPackageName;
	bool bStarted = false;
};
