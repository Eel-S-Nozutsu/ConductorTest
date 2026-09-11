// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ContentDirectorSubsystem.generated.h"

class UContentDirector;
class UDataTable;
class UMapDirector;

/**
 * MapDirectorの生存管理役。
 * レベル(サブレベル含む)のロード/アンロードでDT_MapDirectorListを照合し、MapDirectorを生成・破棄する。
 * ゲーム内容の知識は持たない。
 */
UCLASS()
class PRJ_TIDE_P0_API UContentDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ContentIdでコンテンツを引く。同名は先に見つかったものを返す
	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	UContentDirector* FindContentDirector(FName ContentId) const;

	const TArray<TObjectPtr<UMapDirector>>& GetMapDirectors() const { return MapDirectors; }

private:

	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void HandleLevelRemoved(ULevel* Level, UWorld* World);

	void TryCreateMapDirector(const ULevel* Level);
	void DestroyMapDirector(FName LevelPackageName);

	// PIEの重複パッケージ名(UEDPIE_0_接頭辞)を落としてDTの指定と突き合わせられる形にする
	static FName MakeLevelPackageName(const ULevel* Level);

	void CacheTables();

	UPROPERTY()
	TArray<TObjectPtr<UMapDirector>> MapDirectors;

	// ソフト参照のままだとGCで落ちるので実体を保持する
	UPROPERTY()
	TObjectPtr<UDataTable> MapDirectorTable;

	// BeginPlay前に来たレベルは触らない (OnWorldBeginPlayでまとめて拾う)
	bool bWorldStarted = false;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;

};
