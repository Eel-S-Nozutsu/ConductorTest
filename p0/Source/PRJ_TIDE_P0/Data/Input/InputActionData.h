// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InputActionData.generated.h"

class UInputAction;

/**
 * 入力アクションデータ
 */
USTRUCT( BlueprintType )
struct FInputActionData
{
	GENERATED_BODY()

public:
	// ラベル
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	FString Label;

	// コメント
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	FString Comment;

	// 入力アクション
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	UInputAction* InputAction = nullptr;
};
