// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Movement/AnimNotifyState_SetMovementMode.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UAnimNotifyState_SetMovementMode::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		Character->GetCharacterMovement()->SetMovementMode(ActiveMode);
	}
}

void UAnimNotifyState_SetMovementMode::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		Character->GetCharacterMovement()->SetMovementMode(RestoreMode);
	}
}
