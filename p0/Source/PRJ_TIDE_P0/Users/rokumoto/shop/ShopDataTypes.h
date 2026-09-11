// Copyright (c) 2026, A.Takeuchi EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ShopDataTypes.generated.h"
/**
 * 
 */


 // アイテム
USTRUCT(BlueprintType)
struct FItemMasterRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// アイテムID（FNameにしておくと検索が高速で、検索用Keyとして扱いやすいです）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemMaster")
	FName ItemID;

	// アイテム名（UI表示用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemMaster")
	FText ItemName;

	// アイテム詳細テキスト（UI表示用・複数行対応）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemMaster", meta = (MultiLine = true))
	FText Description;
};

// ショップ用
USTRUCT(BlueprintType)
struct FShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 対象アイテムのID（アイテムマスターのItemIDと一致させます）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ItemID;

	// 在庫数（-1等にして無制限を表す運用もよく使われます）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "-1"))
	int32 Stock = -1;

	// 単価
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0"))
	int32 Price = 0;
};
