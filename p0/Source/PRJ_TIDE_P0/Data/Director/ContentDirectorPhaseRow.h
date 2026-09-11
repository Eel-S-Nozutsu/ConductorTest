// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ContentDirectorPhaseRow.generated.h"

class UDirectorCondition;
class UDirectorPhaseAction;

/**
 * フェーズの出口1本。ConditionがtrueになったらNextPhaseへ進む。
 */
USTRUCT(BlueprintType)
struct FDirectorPhaseTransition
{
	GENERATED_BODY()

	// 実体はフェーズ入場時に生成する。DTが行の複製でサブオブジェクトを共有しないようクラス参照で持つ
	UPROPERTY(EditAnywhere, Category = "Director")
	TSubclassOf<UDirectorCondition> Condition;

	// 遷移先のフェーズ名 (この表の行名)
	UPROPERTY(EditAnywhere, Category = "Director")
	FName NextPhase;

};

/**
 * フェーズ1つの定義。行名がフェーズ名になる。
 * この表はコンテンツごとに1枚なので、コンテンツを指す列は持たない。
 */
USTRUCT(BlueprintType)
struct FContentDirectorPhaseRow : public FTableRowBase
{
	GENERATED_BODY()

	// フェーズ開始時の単発処理。継続処理はModuleに書く
	UPROPERTY(EditAnywhere, Category = "Director")
	TArray<TSubclassOf<UDirectorPhaseAction>> EntryActions;

	// 上から評価して最初に成立した辺を採る。同時成立に備えて成功辺を先に書く
	UPROPERTY(EditAnywhere, Category = "Director")
	TArray<FDirectorPhaseTransition> Transitions;

};
