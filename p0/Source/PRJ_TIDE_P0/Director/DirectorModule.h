// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Director/DirectorObjectBase.h"
#include "DirectorModule.generated.h"

class UContentDirector;
class UMapDirector;

/**
 * ハードコードの置き場。複数セット可。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PRJ_TIDE_P0_API UDirectorModuleBase : public UDirectorObjectBase
{
	GENERATED_BODY()

public:

	// ディレクター側から呼ぶ
	void StartModule();
	void StopModule();
	void TickModule(float DeltaSeconds);

protected:

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnStart();

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnStop();

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnTick(float DeltaSeconds);

};

/**
 * レベル全体に効くロジック
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PRJ_TIDE_P0_API UMapDirectorModule : public UDirectorModuleBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	UMapDirector* GetMapDirector() const;

};

/**
 * 1コンテンツの遊びの中身と、その実行中の可変データ(選んだステータス・残り時間など)の持ち主。
 *
 * 可変データの置き場はここだけ。Condition/ActionからはUContentDirector::FindModuleByClassで読む。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PRJ_TIDE_P0_API UContentDirectorModule : public UDirectorModuleBase
{
	GENERATED_BODY()

public:

	// UContentDirectorから呼ぶ
	void EnterPhase(FName Phase);
	void ExitPhase(FName Phase);

	UFUNCTION(BlueprintPure, Category = "Tide|Director")
	UContentDirector* GetContentDirector() const;

protected:

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnEnterPhase(FName Phase);

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void OnExitPhase(FName Phase);

};
