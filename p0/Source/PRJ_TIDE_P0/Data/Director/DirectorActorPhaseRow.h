// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DirectorActorPhaseRow.generated.h"

/**
 * フェーズ中のアクターの居方
 */
UENUM(BlueprintType)
enum class EDirectorActorPresence : uint8
{
	// 生成する(配置アクターなら表示・有効化して初期状態へ戻す)
	Active,

	// 居るが止める(表示したままTickと当たりを切る)
	Frozen,

	// 隠す(表示と当たりを切る。生成済みの実体は保持)
	Hidden,

	// 片付ける(生成した実体は破棄、配置アクターは非表示)
	Removed,
};

/**
 * (アクター × フェーズ) の差分表。この表はコンテンツごとに1枚。
 * フェーズ別の記述が要らないアクターはこの表に行を書かない(既定はActive)。
 */
USTRUCT(BlueprintType)
struct FDirectorActorPhaseRow : public FTableRowBase
{
	GENERATED_BODY()

	// 対象アクター (台帳の行名) ※未設定ならそのフェーズの全アクターに適用する既定行になる
	UPROPERTY(EditAnywhere, Category = "Director")
	FName ActorId;

	// 対象フェーズ (フェーズ表の行名)
	UPROPERTY(EditAnywhere, Category = "Director")
	FName Phase;

	UPROPERTY(EditAnywhere, Category = "Director")
	EDirectorActorPresence Presence = EDirectorActorPresence::Active;

	// フェーズ入場時に与えるHP割合 ※0以下で変更しない
	UPROPERTY(EditAnywhere, Category = "Director", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthRatio = 0.0f;

};
