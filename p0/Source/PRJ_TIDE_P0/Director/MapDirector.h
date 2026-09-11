// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Director/DirectorObjectBase.h"
#include "MapDirector.generated.h"

class UContentDirector;
class UDataTable;
class UMapDirectorModule;
struct FMapDirectorRow;

/**
 * 1レベル(エリア)に1つ。侵入時に、そのレベルに属するContentDirectorを生成する。
 * 個々のコンテンツの進行には関与せず、レベル全体に効くロジックはUMapDirectorModuleに書く。
 */
UCLASS(BlueprintType)
class PRJ_TIDE_P0_API UMapDirector : public UDirectorObjectBase
{
	GENERATED_BODY()

public:

	// UContentDirectorSubsystemから呼ぶ
	void StartDirector(const FMapDirectorRow& Row, FName InLevelPackageName);
	void StopDirector();
	void TickDirector(float DeltaSeconds, float EvaluateInterval);

	UFUNCTION(BlueprintCallable, Category = "Tide|Director")
	UContentDirector* FindContentDirector(FName ContentId) const;

	FName GetLevelPackageName() const { return LevelPackageName; }

	const TArray<TObjectPtr<UContentDirector>>& GetContentDirectors() const { return ContentDirectors; }

private:

	// ソフト参照のままだとGCで落ちるので実体を保持する
	UPROPERTY()
	TObjectPtr<UDataTable> ContentList;

	UPROPERTY()
	TArray<TObjectPtr<UMapDirectorModule>> Modules;

	UPROPERTY()
	TArray<TObjectPtr<UContentDirector>> ContentDirectors;

	FName LevelPackageName;
	bool  bStarted = false;

};
