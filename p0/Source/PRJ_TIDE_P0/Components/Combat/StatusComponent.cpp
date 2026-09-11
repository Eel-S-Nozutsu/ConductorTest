// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

void UStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;
}

void UStatusComponent::InitializeFromData(const FStatusSettings& Settings)
{
	MaxHP = Settings.MaxHP;
	BaseAttackPower = Settings.BaseAttackPower;
	CurrentHP = MaxHP;
	bIsDead = false;
}

void UStatusComponent::ModifyHP(float Delta)
{
	if (bIsDead) return;

	CurrentHP = FMath::Clamp(CurrentHP + Delta, 0.0f, MaxHP);
	OnHPChanged.Broadcast(CurrentHP, MaxHP);

	if (CurrentHP <= 0.0f)
	{
		bIsDead = true;
		OnDeath.Broadcast();
	}
}

void UStatusComponent::ResetStatus()
{
	bIsDead = false;
	CurrentHP = MaxHP;
	OnHPChanged.Broadcast(CurrentHP, MaxHP);
	OnRevive.Broadcast();
}

void UStatusComponent::Revive(float RestoreRatio)
{
	if (!bIsDead) return;

	bIsDead = false;
	const float TargetHP = MaxHP * FMath::Clamp(RestoreRatio, 0.0f, 1.0f);
	CurrentHP = TargetHP;

	OnHPChanged.Broadcast(CurrentHP, MaxHP);
	OnRevive.Broadcast();
}
