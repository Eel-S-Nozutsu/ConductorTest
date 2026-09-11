// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AnimNotify_ClearStateTag.h"

#include "PRJ_TIDE_P0/Components/State/StateTagComponent.h"

UAnimNotify_ClearStateTag::UAnimNotify_ClearStateTag()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor( 255, 0, 0, 255 );
#endif
}

void UAnimNotify_ClearStateTag::Notify( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference )
{
	Super::Notify( MeshComp, Animation, EventReference );

	if ( MeshComp && MeshComp->GetOwner() )
	{
		if ( UStateTagComponent* TagComp = MeshComp->GetOwner()->FindComponentByClass<UStateTagComponent>() )
		{
			// コンテナ内の全タグを順次削除
			for ( auto It = TargetTags.CreateConstIterator(); It; ++It )
			{
				TagComp->RemoveStateTag( *It );
			}
		}
	}
}
