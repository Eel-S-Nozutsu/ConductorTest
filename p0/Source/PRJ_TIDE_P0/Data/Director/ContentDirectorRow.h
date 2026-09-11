// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ContentDirectorRow.generated.h"

class UContentDirectorModule;

/**
 * コンテンツ1本(修練場1つなど)の定義。行名がContentIdになる。
 * フェーズとアクターの記述はコンテンツごとの表に分かれており、この行がそれを指す。
 */
USTRUCT(BlueprintType)
struct FContentDirectorRow : public FTableRowBase
{
	GENERATED_BODY()

	// フェーズ定義 ※コンテンツごとに1枚
	UPROPERTY(EditAnywhere, Category = "Director", meta = (RowType = "/Script/PRJ_TIDE_P0.ContentDirectorPhaseRow"))
	TSoftObjectPtr<UDataTable> PhaseTable;

	// 起動直後に入るフェーズ (フェーズ表の行名)
	UPROPERTY(EditAnywhere, Category = "Director")
	FName InitialPhase;

	// 管理アクターの台帳 ※未設定でアクター管理なし
	UPROPERTY(EditAnywhere, Category = "Director", meta = (RowType = "/Script/PRJ_TIDE_P0.DirectorActorRow"))
	TSoftObjectPtr<UDataTable> ActorTable;

	// アクターのフェーズ別差分 ※未設定なら全フェーズでActive扱い
	UPROPERTY(EditAnywhere, Category = "Director", meta = (RowType = "/Script/PRJ_TIDE_P0.DirectorActorPhaseRow"))
	TSoftObjectPtr<UDataTable> ActorPhaseTable;

	// 遊びの中身と実行中の可変データの置き場。配列順に生成・実行する
	UPROPERTY(EditAnywhere, Category = "Director")
	TArray<TSubclassOf<UContentDirectorModule>> Modules;

};
