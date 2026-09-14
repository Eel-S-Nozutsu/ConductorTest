// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConductorObjectBase.h"
#include "ConductorPhaseAction.generated.h"

class UContentConductor;

/**
 * フェーズ開始時に一度だけ実行する処理
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class CONDUCTOR_API UConductorPhaseAction : public UConductorObjectBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	void Execute(UContentConductor* Conductor);
};

/**
 * ログ出力
 */
UCLASS(meta = (DisplayName = "ログ出力"))
class CONDUCTOR_API UConductorPhaseAction_Log : public UConductorPhaseAction
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(UContentConductor* Conductor) override;

	UPROPERTY(EditAnywhere, meta = (DisplayName = "出力するログ"))
	FString Message;
};
