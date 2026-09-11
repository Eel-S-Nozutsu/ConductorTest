// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "DirectorSettings.generated.h"

/**
 * ディレクターの入口となるDTと共通ノブ。プロジェクト設定 > Game > Tide Content Director
 *
 * ここで指すのはレベル紐づけの1枚だけ。コンテンツ一覧はレベルごと、フェーズとアクターの記述は
 * コンテンツごとの表に分かれており、それぞれ親の行が指す。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tide Content Director"))
class PRJ_TIDE_P0_API UDirectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UDirectorSettings();

	static const UDirectorSettings* Get() { return GetDefault<UDirectorSettings>(); }

	UPROPERTY(EditAnywhere, Config, Category = "Director", meta = (DisplayName = "レベル紐づけ(全レベル分の1枚)", RowType = "/Script/PRJ_TIDE_P0.MapDirectorRow"))
	TSoftObjectPtr<UDataTable> MapDirectorTable;

	// 遷移条件を評価する間隔。Moduleのtickは毎フレームのままなので、ここは条件だけに効く
	UPROPERTY(EditAnywhere, Config, Category = "Director", meta = (DisplayName = "条件評価の間隔(秒)", ClampMin = "0.0"))
	float ConditionEvaluateInterval = 0.1f;

};
