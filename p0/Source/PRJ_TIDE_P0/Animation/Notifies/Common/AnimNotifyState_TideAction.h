// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_TideAction.generated.h"

class UTideNotifyBehavior;
class ATideCharacter;

UCLASS(meta = (DisplayName = "キャラクターアクション"))
class PRJ_TIDE_P0_API UAnimNotifyState_TideAction : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UAnimNotifyState_TideAction();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:

	UPROPERTY(EditAnywhere, Instanced)
	TArray<TObjectPtr<UTideNotifyBehavior>> Behaviors;

private:

	ATideCharacter* GetCharacter(USkeletalMeshComponent* MeshComp) const;

};
