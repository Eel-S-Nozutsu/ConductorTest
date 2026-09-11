// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_HaloAttach.generated.h"

UCLASS(meta = (DisplayName = "Halo Attach", Category = "Equipment"))
class PRJ_TIDE_P0_API UAnimNotifyState_HaloAttach : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	// 光輪メッシュコンポーネント名
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName HaloComponentName = TEXT("Halo");

	// アタッチ先ソケット名 ※モンタージュごとに設定
	UPROPERTY(EditAnywhere, Category = "Halo")
	FName TargetSocketName = TEXT("Center_Slash_FX");

	// 光輪を明るくするか ※攻撃=true/ガード展開=false
	UPROPERTY(EditAnywhere, Category = "Halo")
	bool bBrightGlow = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:

	void ApplySocket(USkeletalMeshComponent* MeshComp, FName SocketName) const;

};
