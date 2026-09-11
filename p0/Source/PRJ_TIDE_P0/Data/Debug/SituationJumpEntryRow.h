// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SituationJumpEntryRow.generated.h"

USTRUCT(BlueprintType)
struct FSituationJumpEntryRow : public FTableRowBase
{
	GENERATED_BODY()

	// 省略した場合は行名を表示名として使用する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SituationJump")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SituationJump")
	FName LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SituationJump")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SituationJump")
	float Yaw = 0.0f;

};
