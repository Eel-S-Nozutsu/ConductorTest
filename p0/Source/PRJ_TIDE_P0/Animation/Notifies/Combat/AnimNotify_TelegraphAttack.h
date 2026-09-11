// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_TelegraphAttack.generated.h"

/**
 * プレイヤー攻撃アニメーションのヒットボックス発生前に配置し、
 * 近隣のAEnemyCharacterに予知回避を促すNotify
 */
UCLASS(meta = (DisplayName = "TelegraphAttack", Category = "Combat"))
class PRJ_TIDE_P0_API UAnimNotify_TelegraphAttack : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 敵が反応できる距離
	UPROPERTY(EditAnywhere)
	float DetectionRadius = 300.0f;

};
