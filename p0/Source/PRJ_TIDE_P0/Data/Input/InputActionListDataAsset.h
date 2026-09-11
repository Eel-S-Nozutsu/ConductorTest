// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "InputActionData.h"

#include "InputActionListDataAsset.generated.h"

/**
 * 入力アクションデータリスト
 */
UCLASS()
class PRJ_TIDE_P0_API UInputActionListDataAsset : public UPrimaryDataAsset
{
private:
	GENERATED_BODY()

public:
	UPROPERTY( EditDefaultsOnly )
	TArray<TObjectPtr<UInputAction>> Actions;
};
