// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "AnimMontageListDataAsset.generated.h"

// 各アニメーション要素の定義
USTRUCT( BlueprintType )
struct FAnimEntry
{
	GENERATED_BODY()

public:
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	FName Comment;

	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TObjectPtr<UAnimMontage> AnimMontage = nullptr;
};

// カテゴリ化するための構造体
USTRUCT( BlueprintType )
struct FAnimCategory
{
	GENERATED_BODY()

public:
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TMap<FName, FAnimEntry> Entries;
};

// DataAsset クラス
// todo: AnimMontageDataAssetに改名したい
UCLASS()
class PRJ_TIDE_P0_API UAnimMontageListDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// TMapのキーにカテゴリ名を設定することで、エディタ上で分かりやすいヘッダーに
	UPROPERTY( EditAnywhere, BlueprintReadOnly )
	TMap<FName, FAnimCategory> Categories;

	UAnimMontage* GetAnimMontage( const FName& Label );
};
