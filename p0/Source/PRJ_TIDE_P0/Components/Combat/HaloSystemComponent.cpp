// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/HaloSystemComponent.h"
#include "Components/PrimitiveComponent.h"

namespace
{
	// 防御(Deployed/Guarding)＝光輪が部位を守っている
	// それ以外(Away/Broken/Reforming)は無防備
	bool IsDefendedState(EHaloUnitState State)
	{
		return State == EHaloUnitState::Deployed || State == EHaloUnitState::Guarding;
	}
}

UHaloSystemComponent::UHaloSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHaloSystemComponent::Initialize(const TArray<FHaloUnitConfig>& InUnits)
{
	Units.Reset();
	Units.Reserve(InUnits.Num());

	for (const FHaloUnitConfig& Config : InUnits)
	{
		if (Config.PartTag.IsNone()) continue;
		Units.FindOrAdd(Config.PartTag).Config = Config;
	}

	// 自前再生(ザコ)のときだけTickする。ボスはPartDestructionがミラー駆動
	SetComponentTickEnabled(bAutoRegen && Units.Num() > 0);
}

void UHaloSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAutoRegen) return;

	for (TPair<FName, FHaloUnit>& Pair : Units)
	{
		FHaloUnit& Unit = Pair.Value;
		if (Unit.State != EHaloUnitState::Broken) continue;

		Unit.RegenTimer -= DeltaTime;
		if (Unit.RegenTimer <= 0.0f)
		{
			Unit.State = EHaloUnitState::Deployed;
		}
	}
}

float UHaloSystemComponent::GetPartDamageMultiplier(FName PartTag) const
{
	const FHaloUnit* Unit = Units.Find(PartTag);
	if (!Unit) return 1.0f;

	// 防御＝光輪ヒット(小)。無防備＝本体ヒット(大)
	return IsDefendedState(Unit->State)
		? Unit->Config.HaloHitDamageMultiplier
		: Unit->Config.BodyHitDamageMultiplier;
}

bool UHaloSystemComponent::IsPartDefenseless(FName PartTag) const
{
	const FHaloUnit* Unit = Units.Find(PartTag);
	return Unit && !IsDefendedState(Unit->State);
}

bool UHaloSystemComponent::HasUnit(FName PartTag) const
{
	return Units.Contains(PartTag);
}

FName UHaloSystemComponent::GetUnitPartTagForComponent(const UPrimitiveComponent* HitComp) const
{
	if (!HitComp) return NAME_None;

	for (const FName& Tag : HitComp->ComponentTags)
	{
		if (Units.Contains(Tag)) return Tag;
	}
	return NAME_None;
}

void UHaloSystemComponent::SetUnitState(FName PartTag, EHaloUnitState NewState)
{
	FHaloUnit* Unit = Units.Find(PartTag);
	if (!Unit) return;

	Unit->State = NewState;

	// 自前で再生を回すユニット(ザコ)のみ、破壊は破壊クールで再生する(技クールではなく)
	if (bAutoRegen && NewState == EHaloUnitState::Broken)
	{
		Unit->RegenTimer = Unit->Config.BreakRegenCooldown;
		SetComponentTickEnabled(true);
	}
}

EHaloUnitState UHaloSystemComponent::GetUnitState(FName PartTag) const
{
	const FHaloUnit* Unit = Units.Find(PartTag);
	return Unit ? Unit->State : EHaloUnitState::Deployed;
}
