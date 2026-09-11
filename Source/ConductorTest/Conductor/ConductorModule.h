// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Conductor/ConductorObjectBase.h"
#include "ConductorModule.generated.h"

class UContentConductor;
class UMapConductor;

/**
 * ハードコードする場所
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTORTEST_API UConductorModuleBase : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	void StartModule();
	void StopModule();
	void TickModule(float DeltaSeconds);

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnStart();

	UFUNCTION(BlueprintNativeEvent)
	void OnStop();

	UFUNCTION(BlueprintNativeEvent)
	void OnTick(float DeltaSeconds);
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTORTEST_API UMapConductorModule : public UConductorModuleBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	UMapConductor* GetMapConductor() const;
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTORTEST_API UContentConductorModule : public UConductorModuleBase
{
	GENERATED_BODY()

public:
	void EnterPhase(FName Phase);
	void ExitPhase(FName Phase);

	UFUNCTION(BlueprintPure)
	UContentConductor* GetContentConductor() const;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnEnterPhase(FName Phase);

	UFUNCTION(BlueprintNativeEvent)
	void OnExitPhase(FName Phase);
};
