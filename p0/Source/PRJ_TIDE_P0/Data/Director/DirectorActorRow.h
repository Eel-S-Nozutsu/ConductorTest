// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DirectorActorRow.generated.h"

/**
 * ディレクターが管理するアクター1体の台帳。行名がActorIdになる。
 *
 * この表はコンテンツごとに1枚。フェーズごとの扱いは別表(FDirectorActorPhaseRow)に書く。
 * レベル配置アクターを指す場合は、そのアクターにUDirectorIdComponentを付けて行名と一致させる
 * (DTはアセットなので、レベル内アクターを直接参照できない)。
 */
USTRUCT(BlueprintType)
struct FDirectorActorRow : public FTableRowBase
{
	GENERATED_BODY()

	// 「このグループの敵をN体倒したら」のような条件がまとめて数えるための分類
	UPROPERTY(EditAnywhere, Category = "Director")
	FName GroupId;

	// 生成するクラス ※未設定ならレベル配置アクター(UDirectorIdComponent)を探す
	UPROPERTY(EditAnywhere, Category = "Director")
	TSubclassOf<AActor> SpawnClass;

	// 生成位置を借りるレベル配置アクターのActorId ※SpawnClass使用時のみ
	UPROPERTY(EditAnywhere, Category = "Director")
	FName SpawnPointId;

};
