// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"

#include "AnimNotify_ShiftUpGear.generated.h"

UCLASS( meta = ( DisplayName = "Shift Up Gear", Category = "Player" ) )
class UAnimNotify_ShiftUpGear : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference ) override;

};
