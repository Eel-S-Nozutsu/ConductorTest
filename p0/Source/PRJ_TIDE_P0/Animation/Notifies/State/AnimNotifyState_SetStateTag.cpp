// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AnimNotifyState_SetStateTag.h"

#include "PRJ_TIDE_P0/Components/State/StateTagComponent.h"

UAnimNotifyState_SetStateTag::UAnimNotifyState_SetStateTag()
{
	// モンタージュ上で見やすくなるようにデフォルト色を設定
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor( 128, 255, 128, 255 );
#endif
}

void UAnimNotifyState_SetStateTag::NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference )
{
	Super::NotifyBegin( MeshComp, Animation, TotalDuration, EventReference );

	if ( !MeshComp ) return;

	if ( AActor* OwnerActor = MeshComp->GetOwner() )
	{
		if ( UStateTagComponent* TagComp = OwnerActor->FindComponentByClass<UStateTagComponent>() )
		{
			if ( StateTags.IsValid() )
			{
				for ( const FGameplayTag& Tag : StateTags )
				{
					TagComp->AddStateTag( Tag );
				}
			}
		}
	}
}

void UAnimNotifyState_SetStateTag::NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference )
{
	Super::NotifyEnd( MeshComp, Animation, EventReference );

	if ( !MeshComp || !StateTags.IsValid() ) return;

	if ( AActor* OwnerActor = MeshComp->GetOwner() )
	{
		if ( UStateTagComponent* TagComp = OwnerActor->FindComponentByClass<UStateTagComponent>() )
		{
			for ( const FGameplayTag& Tag : StateTags )
			{
				TagComp->RemoveStateTag( Tag );
			}
		}
	}
}
