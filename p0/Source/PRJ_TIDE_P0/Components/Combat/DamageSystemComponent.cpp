// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"

#if !UE_BUILD_SHIPPING
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/DamageNumberDebug.h"
#include "GameFramework/Pawn.h"
#endif

void UDamageSystemComponent::Initialize(UStatusComponent* InStatus)
{
	StatusComponent = InStatus;
}

void UDamageSystemComponent::ProcessDamage(const FDamageInfo& DamageInfo)
{
	if (!ValidateDamage(DamageInfo)) return;

	if (StatusComponent)
	{
		StatusComponent->ModifyHP(-DamageInfo.BaseDamage);
	}

#if !UE_BUILD_SHIPPING
	// デバッグ用ダメージ数値オーバーレイへ通知(フラグOFF時はPush側で無視される)
	// 被弾側＝このComponentの所有アクター。プレイヤー操作のPawnかどうかで輪郭色を分ける
	if (const AActor* Victim = GetOwner())
	{
		const APawn* VictimPawn = Cast<APawn>(Victim);
		const bool bVictimIsPlayer = VictimPawn && VictimPawn->IsPlayerControlled();

		FVector PopupLocation = Victim->GetActorLocation();
		if (DamageInfo.HitResult.bBlockingHit)
		{
			PopupLocation = DamageInfo.HitResult.ImpactPoint;
		}
		PopupLocation.Z += 50.0f;	// 少し上に出す

		FDamageNumberDebug::Push(PopupLocation, DamageInfo.BaseDamage, bVictimIsPlayer);
	}
#endif

	OnDamageReceived.Broadcast(DamageInfo);
}

bool UDamageSystemComponent::ValidateDamage(const FDamageInfo& DamageInfo) const
{
	if (!StatusComponent) return false;
	if (StatusComponent->IsDead()) return false;
	return true;
}
