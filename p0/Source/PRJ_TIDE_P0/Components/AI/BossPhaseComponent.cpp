// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/AI/BossPhaseComponent.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

void UBossPhaseComponent::Initialize(const UBossDataAsset* InData)
{
	Data = InData;

	AEnemyCharacter* Owner = Cast<AEnemyCharacter>(GetOwner());
	if (!Owner) return;

	// HP変化でフェーズ遷移を判定
	if (UStatusComponent* Status = Owner->FindComponentByClass<UStatusComponent>())
	{
		Status->OnHPChanged.AddDynamic(this, &UBossPhaseComponent::OnHPChanged);
	}

	// 部位破壊でフェーズ遷移を判定
	if (UPartDestructionComponent* PartComp = Owner->FindComponentByClass<UPartDestructionComponent>())
	{
		PartComp->OnPartDestroyed.AddUObject(this, &UBossPhaseComponent::HandlePartDestroyed);
	}
}

int32 UBossPhaseComponent::GetTotalPhaseCount() const
{
	return Data ? Data->Phases.Num() : 0;
}

void UBossPhaseComponent::HandleAttackEvent(FGameplayTag Tag)
{
	/*if (Tag == TAG_Boss_Phase_Next)
	{
		TransitionToPhase(CurrentPhaseIndex + 1);
	}*/
}

void UBossPhaseComponent::OnHPChanged(float NewHP, float MaxHP)
{
	if (!Data || Data->Phases.IsEmpty()) return;

	// 現フェーズより後ろのフェーズで、HP閾値を下回った最初のフェーズへ遷移
	const float HPRatio = (MaxHP > 0.0f) ? (NewHP / MaxHP) : 0.0f;

	for (int32 i = CurrentPhaseIndex + 1; i < Data->Phases.Num(); ++i)
	{
		if (HPRatio <= Data->Phases[i - 1].HPThreshold)
		{
			TransitionToPhase(i);
			return;
		}
	}
}

void UBossPhaseComponent::HandlePartDestroyed(FName PartTag)
{
	if (!Data || CurrentPhaseIndex >= Data->Phases.Num() - 1) return;

	// 現在フェーズのPartTriggerTagと一致すれば次フェーズへ
	if (Data->Phases[CurrentPhaseIndex].PartTriggerTag == PartTag)
	{
		TransitionToPhase(CurrentPhaseIndex + 1);
	}
}

void UBossPhaseComponent::TransitionToPhase(int32 NewPhaseIndex)
{
	if (!Data) return;
	if (NewPhaseIndex <= CurrentPhaseIndex) return;
	if (NewPhaseIndex >= Data->Phases.Num()) return;

	const int32 OldPhase = CurrentPhaseIndex;
	CurrentPhaseIndex = NewPhaseIndex;

	// BattleComponentにフェーズを通知してTryPickAttackのフィルターを更新
	if (AEnemyCharacter* Owner = Cast<AEnemyCharacter>(GetOwner()))
	{
		Owner->GetBattleComponent()->SetCurrentPhase(CurrentPhaseIndex);
	}

	OnPhaseChanged.Broadcast(OldPhase, CurrentPhaseIndex);
}
