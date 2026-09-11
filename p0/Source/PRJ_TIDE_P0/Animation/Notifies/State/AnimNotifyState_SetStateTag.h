// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"

#include "AnimNotifyState_SetStateTag.generated.h"

UCLASS( meta = ( DisplayName = "Set State Tag", Category = "State" ))
class UAnimNotifyState_SetStateTag : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_SetStateTag();

	virtual void NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference ) override;
//	virtual void NotifyTick( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference ) override;
	virtual void NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference ) override;

public:
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|State Tags", meta = ( Categories = "State" ) )
	FGameplayTagContainer StateTags;
};
