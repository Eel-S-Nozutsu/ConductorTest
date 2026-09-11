// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemySpawnTableRow.generated.h"

class AEnemyCharacter;

USTRUCT(BlueprintType)
struct FEnemySpawnTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AEnemyCharacter> ActorClass;

	// 省略した場合は行名を表示名として使用する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FString DisplayName;

};
