// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ConductorSubsystem.generated.h"

class UContentConductor;
class UMapConductor;

/**
 * 
 */
UCLASS()
class CONDUCTORTEST_API UConductorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	bool ShouldCreateSubsystem(UObject* Outer) const override;
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	void OnWorldBeginPlay(UWorld& InWorld) override;

	void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;

	// ContentIdでコンテンツコンダクタを探す
	UFUNCTION(BlueprintCallable)
	UContentConductor* FindContentConductor(FName ContentId) const;

	const TArray<TObjectPtr<UMapConductor>>& GetMapConductors() const { return MapConductors; }

private:
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void HandleLevelRemoved(ULevel* Level, UWorld* World);

	void TryCreateMapConductor(const ULevel* Level);
	void DestroyMapConductor(FName LevelPackageName);

	static FName MakeLevelPackageName(const ULevel* Level); // "PIE_"外す

	void CacheTables();

	UPROPERTY()
	TArray<TObjectPtr<UMapConductor>> MapConductors;

	UPROPERTY()
	TObjectPtr<UDataTable> MapConductorTable;

	bool bWorldStarted = false;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
};
