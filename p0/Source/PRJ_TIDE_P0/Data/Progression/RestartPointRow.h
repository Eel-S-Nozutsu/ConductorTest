// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RestartPointRow.generated.h"

/**
 * リスタート地点1件分の定義。
 * RowNameをキーに、ARestartTriggerVolumeがFDataTableRowHandleで対応行を指す。
 */
USTRUCT(BlueprintType)
struct FRestartPointRow : public FTableRowBase
{
	GENERATED_BODY()

	// 進行順 (大きいほど後のチェックポイント)
	// リスタート地点は前進のみ更新し、現在より小さい順番のトリガーでは上書きしない
	// (Uターンで手前のトリガーに戻っても復帰点が巻き戻らないようにするため)
	UPROPERTY(EditAnywhere, Category = "Restart")
	int32 ProgressOrder = 0;

	// リスタート時の復帰座標 (ワールド)
	UPROPERTY(EditAnywhere, Category = "Restart")
	FVector Location = FVector::ZeroVector;

	// リスタート時の復帰向き
	UPROPERTY(EditAnywhere, Category = "Restart")
	FRotator Rotation = FRotator::ZeroRotator;

};
