// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AnimNotify_ClearStateTag.generated.h"

UCLASS( meta = ( DisplayName = "Clear State Tag", Category = "State" ) )
class UAnimNotify_ClearStateTag : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ClearStateTag();

//	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference ) override;

protected:
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|State", meta = ( AllowPrivateAccess = "true", Categories = "State" ) )
	FGameplayTagContainer TargetTags;
};
