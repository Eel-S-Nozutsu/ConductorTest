// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotify_ChanceMontage.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_ChanceMontage::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !Montage) return;

	// 抽選。Chance=1ならFRand()<1で必ず、Chance=0なら常に外れ
	if (FMath::FRand() >= Chance) return;

	if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
	{
		// 同スロットなら再生中のモンタージュ(のけぞり等)を中断して差し替わる
		// 中断側はbInterrupted扱いでAI再開されず、この遷移先の自然終了で再開される
		AnimInst->Montage_Play(Montage, PlayRate);
	}
}
