// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NiagaraSystem.h"

#include "NiagaraSystemListDataAsset.generated.h"

// 各アニメーション要素の定義
USTRUCT( BlueprintType )
struct FNiagaraEntry
{
	GENERATED_BODY()

public:
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	FName Comment;

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;
};

// カテゴリ化するための構造体
USTRUCT( BlueprintType )
struct FNiagaraCategory
{
	GENERATED_BODY()

public:
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TMap<FName, FNiagaraEntry> Entries;
};

// DataAsset クラス
// todo: NiagaraSystemDataAssetに改名したい
UCLASS()
class PRJ_TIDE_P0_API UNiagaraSystemListDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// TMapのキーにカテゴリ名を設定することで、エディタ上で分かりやすいヘッダーに
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TMap<FName, FNiagaraCategory> Categories;

	UNiagaraSystem* GetNiagaraSystem( const FName& Label );
};
