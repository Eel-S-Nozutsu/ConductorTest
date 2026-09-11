// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AnimNotify_ShiftUpGear.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

void UAnimNotify_ShiftUpGear::Notify( USkeletalMeshComponent* MeshComp,	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference )
{
	Super::Notify( MeshComp, Animation, EventReference );

	if ( !MeshComp ) return;

	if ( ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>( MeshComp->GetOwner() ) )
	{
		Player->RequestShiftUpChargeGear();
	}
}
