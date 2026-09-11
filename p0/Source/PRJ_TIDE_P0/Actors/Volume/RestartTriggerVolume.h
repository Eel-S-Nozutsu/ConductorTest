// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "Engine/DataTable.h"
#include "RestartTriggerVolume.generated.h"

/**
 * リスタート地点を切り替えるトリガーボリューム。
 * プレイヤーが進入すると、RestartPointが指すDT_RestartPointsの行を解決し、
 * プレイヤーのURestartComponentに現在のリスタート地点として設定する。
 * 坂・エリアの区切りごとに1つ手置きし、各トリガーのRestartPointに対応行を割り当てる。
 */
UCLASS()
class PRJ_TIDE_P0_API ARestartTriggerVolume : public ATriggerVolume
{
	GENERATED_BODY()

protected:

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	// このトリガー進入時に採用するリスタート地点 (DT_RestartPointsの行)
	UPROPERTY(EditInstanceOnly, Category = "Tide|Restart", meta = (RowType = "/Script/PRJ_TIDE_P0.RestartPointRow"))
	FDataTableRowHandle RestartPoint;

};
