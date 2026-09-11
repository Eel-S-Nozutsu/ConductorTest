// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "AnimNotifyState_TideAction.h"
#include "TideNotifyBehavior.h"

#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"

UAnimNotifyState_TideAction::UAnimNotifyState_TideAction()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(100, 200, 255, 255);
#endif
}

ATideCharacter* UAnimNotifyState_TideAction::GetCharacter(USkeletalMeshComponent* MeshComp) const
{
	return MeshComp ? Cast<ATideCharacter>(MeshComp->GetOwner()) : nullptr;
}

void UAnimNotifyState_TideAction::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ATideCharacter* Character = GetCharacter(MeshComp);
	if (!Character) return;

	for (UTideNotifyBehavior* Behavior : Behaviors)
	{
		if (Behavior) Behavior->OnBegin(Character, TotalDuration);
	}
}

void UAnimNotifyState_TideAction::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	ATideCharacter* Character = GetCharacter(MeshComp);
	if (!Character) return;

	for (UTideNotifyBehavior* Behavior : Behaviors)
	{
		if (Behavior) Behavior->OnTick(Character, FrameDeltaTime);
	}
}

void UAnimNotifyState_TideAction::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	ATideCharacter* Character = GetCharacter(MeshComp);
	if (!Character) return;

	for (UTideNotifyBehavior* Behavior : Behaviors)
	{
		if (Behavior) Behavior->OnEnd(Character);
	}
}
