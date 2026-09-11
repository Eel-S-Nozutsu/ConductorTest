// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Equipment/AnimNotifyState_HaloAttach.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_HaloAttach::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	ApplySocket(MeshComp, TargetSocketName);

	if (MeshComp)
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
		{
			Enemy->SetHaloAttackGlow(bBrightGlow);
			// 光輪攻撃の区間中は共通仕様で光輪を破壊させない
			Enemy->SetHaloAttackSuppressed(true);
		}
	}
}

void UAnimNotifyState_HaloAttach::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
	{
		// 破壊保護を先に解除してからソケットを戻す
		Enemy->SetHaloAttackSuppressed(false);
		Enemy->RestoreHaloToStateSocket(HaloComponentName);
	}
}

FString UAnimNotifyState_HaloAttach::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("HaloAttach [%s]"), *TargetSocketName.ToString());
}

void UAnimNotifyState_HaloAttach::ApplySocket(USkeletalMeshComponent* MeshComp, FName SocketName) const
{
	if (!MeshComp || SocketName.IsNone()) return;

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(MeshComp->GetOwner()))
	{
		Enemy->AttachHaloForAttack(SocketName, HaloComponentName);
	}
}
