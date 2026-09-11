// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "TideCharacterDataAsset.generated.h"

/**
 * キャラクターデータアセット
 */
UCLASS()
class PRJ_TIDE_P0_API UTideCharacterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// ステータス
	UPROPERTY(EditAnywhere, Category = "Status")
	FStatusSettings StatusSettings;

	// ダメージリアクション
	UPROPERTY(EditAnywhere, Category = "Reaction")
	FHitReactionSettings HitReactionSettings;

	// アタックパラメータ(GameplayTagのTagNameを行名として参照)
	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<class UDataTable> AttackParameterTable = nullptr;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("CharacterData", GetFName());
	}

};
