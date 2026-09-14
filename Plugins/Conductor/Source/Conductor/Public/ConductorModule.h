// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConductorObjectBase.h"
#include "ConductorModule.generated.h"

class UContentConductor;
class UMapConductor;

/**
 * コンダクタにロジックを差し込む拡張部品
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTOR_API UConductorModuleBase : public UConductorObjectBase
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
 * 1レベル単位のモジュール
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTOR_API UMapConductorModule : public UConductorModuleBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure)
	UMapConductor* GetMapConductor() const;
};

/**
 * 1コンテンツ単位のモジュール
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CONDUCTOR_API UContentConductorModule : public UConductorModuleBase
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
