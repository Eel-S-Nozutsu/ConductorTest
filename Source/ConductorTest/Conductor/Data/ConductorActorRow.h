// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ConductorActorRow.generated.h"

/**
 * 状態(居方)
 */
UENUM(BlueprintType)
enum class EConductorActorState : uint8
{
	Active UMETA(DisplayName = "出す"),
	//Frozen UMETA(DisplayName = "止める"),
	Hidden UMETA(DisplayName = "隠す"),
	Removed UMETA(DisplayName = "片づける"),
};

/**
 * フェーズでの状態
 */
USTRUCT(BlueprintType)
struct FConductorActorPhaseEntry
{
	GENERATED_BODY()

	// フェーズ表の行名
	UPROPERTY(EditAnywhere)
	FName Phase;

	// 状態
	UPROPERTY(EditAnywhere)
	EConductorActorState State = EConductorActorState::Active;
};

/**
 * コンテンツコンダクタが管理するアクターの定義
 */
USTRUCT(BlueprintType)
struct FConductorActorRow : public FTableRowBase
{
	GENERATED_BODY()

	// コンテンツID
	UPROPERTY(EditAnywhere)
	FName ContentId;

	// 配置アクターを差すId
	UPROPERTY(EditAnywhere)
	FName ActorId;

	// 所属グループ
	UPROPERTY(EditAnywhere)
	FName GroupId;

	// 状態
	UPROPERTY(EditAnywhere)
	TArray<FConductorActorPhaseEntry> Phases;
};
