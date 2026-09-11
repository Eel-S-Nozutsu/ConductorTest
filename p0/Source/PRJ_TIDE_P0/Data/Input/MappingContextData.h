// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MappingContextData.generated.h"

class UInputMappingContext;
class UInputActionListDataAsset;

/**
 * 入力データ
 */
USTRUCT( BlueprintType )
struct FMappingContextData
{
	GENERATED_BODY()

public:
	// 実際のキー割り当て
	UPROPERTY( EditAnywhere )
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	// アクションリスト
	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputActionListDataAsset> ActionList = nullptr;

	// ラベルタグ
	UPROPERTY( EditAnywhere, meta = ( Categories = "Input.Layer" ) )
	FGameplayTag LayerTag;

	// 優先度
	UPROPERTY( EditAnywhere )
	int32 Priority = 0;

	// ゲーム開始時に自動で有効化するか
	UPROPERTY( EditAnywhere )
	bool bAutoActivate = false;
};
