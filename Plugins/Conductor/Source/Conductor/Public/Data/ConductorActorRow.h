// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ConductorActorRow.generated.h"

/**
 * 状態(居方)
 */
UENUM(BlueprintType)
enum class EConductorActorState : uint8
{
	// HiddeninGame(false), Collision(true), Tick(true)
	Active UMETA(DisplayName = "出す"),

	// HiddeninGame(true), Collision(false), Tick(true)
	Hidden UMETA(DisplayName = "隠す"),

	// HiddeninGame(false), Collision(true), Tick(false)
	Frozen UMETA(DisplayName = "止める"),

	// HiddeninGame(true), Collision(false), Tick(false)
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

	// 所属グループ
	UPROPERTY(EditAnywhere)
	FName GroupId;

	// 生成する場合 生成するクラスを指定
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> SpawnClass;

	// 生成する場合 生成先の配置アクターを差すId
	UPROPERTY(EditAnywhere)
	FName SpawnPointId;

	// 状態
	UPROPERTY(EditAnywhere)
	TArray<FConductorActorPhaseEntry> Phases;
};
