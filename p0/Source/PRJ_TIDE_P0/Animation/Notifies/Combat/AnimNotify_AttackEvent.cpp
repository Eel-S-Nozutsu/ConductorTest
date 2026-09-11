// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotify_AttackEvent.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"

void UAnimNotify_AttackEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !EventTag.IsValid()) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	if (UEnemyBattleComponent* Battle = Owner->FindComponentByClass<UEnemyBattleComponent>())
	{
		Battle->DispatchAttackEvent(EventTag);
	}
}
