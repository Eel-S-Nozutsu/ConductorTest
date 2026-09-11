// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MapDirectorRow.generated.h"

class UMapDirectorModule;

/**
 * レベルとMapDirectorの紐づけ。UContentDirectorSubsystemがレベルのロードで照合する。
 * 全レベルぶんをこの1枚に並べる。
 */
USTRUCT(BlueprintType)
struct FMapDirectorRow : public FTableRowBase
{
	GENERATED_BODY()

	// 対象レベル ※サブレベル構成なので、コンテンツを配置する実レベル(PLNなど)を指す
	UPROPERTY(EditAnywhere, Category = "Director")
	TSoftObjectPtr<UWorld> Level;

	// このレベルのコンテンツ一覧 ※レベルごとに1枚
	UPROPERTY(EditAnywhere, Category = "Director", meta = (RowType = "/Script/PRJ_TIDE_P0.ContentDirectorRow"))
	TSoftObjectPtr<UDataTable> ContentList;

	// レベル全体に効くロジック。配列順に生成・実行する
	UPROPERTY(EditAnywhere, Category = "Director")
	TArray<TSubclassOf<UMapDirectorModule>> Modules;

};
