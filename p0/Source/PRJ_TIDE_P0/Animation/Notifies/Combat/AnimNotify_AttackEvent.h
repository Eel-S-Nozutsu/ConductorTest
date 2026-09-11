// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_AttackEvent.generated.h"

/**
 * モンタージュ中の任意フレームでGameplayTagを発火する汎用AttackEventNotify
 * 召喚・投射物スポーン・特殊演出など攻撃に関連するあらゆるイベントに使用
 * UEnemyBattleComponent::OnAttackEventデリゲートへ通知する
 */
UCLASS(meta = (DisplayName = "AttackEvent", Category = "Combat"))
class PRJ_TIDE_P0_API UAnimNotify_AttackEvent : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 発火するイベントタグ
	UPROPERTY(EditAnywhere)
	FGameplayTag EventTag;

};
