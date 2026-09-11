// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeathRagdollTypes.generated.h"

/**
 * 死亡ラグドールの回り方。軸の取り方だけの違いで見た目が変わる。
 * 敵ごとの既定はEnemyDataAssetのDeathSpin、攻撃側からの上書きはFDamageInfoが運ぶ
 */
UENUM(BlueprintType)
enum class EDeathRagdollSpinMode : uint8
{
	None      UMETA(DisplayName = "回転なし"),
	Tumble    UMETA(DisplayName = "前転/横転"),
	Corkscrew UMETA(DisplayName = "きりもみ"),
	Random    UMETA(DisplayName = "ランダム"),
};
