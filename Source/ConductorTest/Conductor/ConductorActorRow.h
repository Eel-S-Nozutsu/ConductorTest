// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ConductorActorRow.generated.h"

/**
 * コンテンツコンダクタが管理するアクターの定義
 */
USTRUCT(BlueprintType)
struct FConductorActorRow : public FTableRowBase
{
	GENERATED_BODY()

	// コンテンツID ※ContentConductorListの行名と対応
	UPROPERTY(EditAnywhere)
	FName ContentId;

	// コンテンツ内の一意な名前
	UPROPERTY(EditAnywhere)
	FName ActorId;

	// 所属グループ
	UPROPERTY(EditAnywhere) // , meta = (Categories = "HitReaction") これで絞る
	FGameplayTag GroupTag;

	// 生成するクラス ※未設定ならレベル配置アクター(UConductorIdComponent)を探す
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> SpawnClass;

	// SpawnClass使用時どこに生成するか
	UPROPERTY(EditAnywhere)
	FName SpawnPointId;
};
