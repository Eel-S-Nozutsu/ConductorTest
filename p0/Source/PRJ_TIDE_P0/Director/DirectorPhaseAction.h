// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Director/DirectorObjectBase.h"
#include "DirectorPhaseAction.generated.h"

class UContentDirector;

/**
 * フェーズ開始時に一度だけ実行する処理。継続処理はUContentDirectorModuleに書く。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class PRJ_TIDE_P0_API UDirectorPhaseAction : public UDirectorObjectBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, Category = "Tide|Director")
	void Execute(UContentDirector* Director);

};

/**
 * シグナルバスへ発火する。扉などの既存USignalReceiverComponentへ疎結合で繋ぐ用
 */
UCLASS(meta = (DisplayName = "シグナル発火"))
class PRJ_TIDE_P0_API UDirectorPhaseAction_BroadcastSignal : public UDirectorPhaseAction
{
	GENERATED_BODY()

public:

	virtual void Execute_Implementation(UContentDirector* Director) override;

	UPROPERTY(EditAnywhere, Category = "Director")
	FName SignalName;

};

/**
 * ログ出力のみ。遷移の確認用
 */
UCLASS(meta = (DisplayName = "ログ出力"))
class PRJ_TIDE_P0_API UDirectorPhaseAction_Log : public UDirectorPhaseAction
{
	GENERATED_BODY()

public:

	virtual void Execute_Implementation(UContentDirector* Director) override;

	UPROPERTY(EditAnywhere, Category = "Director")
	FString Message;

};
