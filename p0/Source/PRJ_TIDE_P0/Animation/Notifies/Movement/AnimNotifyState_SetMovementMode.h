// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AnimNotifyState_SetMovementMode.generated.h"

/**
 * 指定区間中だけMovementModeを切り替える汎用NotifyState
 * ルートモーション吹き飛びなど、Z方向の移動を許可したい区間に配置する
 */
UCLASS(meta = (DisplayName = "SetMovementMode", Category = "Movement"))
class PRJ_TIDE_P0_API UAnimNotifyState_SetMovementMode : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	// 区間中に適用するMovementMode
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EMovementMode> ActiveMode = MOVE_Flying;

	// 区間終了後に戻すMovementMode
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EMovementMode> RestoreMode = MOVE_Falling;

};
