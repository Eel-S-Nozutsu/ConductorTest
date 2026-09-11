// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BossPhaseComponent.generated.h"

class UBossDataAsset;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBossPhaseChanged, int32 /*OldPhase*/, int32 /*NewPhase*/)

/**
 * ボスのフェーズ状態を管理するコンポーネント
 * HP閾値またはGameplayTagで次フェーズへ遷移する
 */
UCLASS()
class PRJ_TIDE_P0_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// ABossCharacter::BeginPlayから呼ぶ
	void Initialize(const UBossDataAsset* InData);

	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }
	int32 GetTotalPhaseCount() const;

	// フェーズ変化通知。ABossCharacterがBBへの書き込みと遷移モンタージュ再生に使用
	FOnBossPhaseChanged OnPhaseChanged;

	// OnAttackEventからGameplayTagで任意フェーズへ強制移行するタグ
	// 例: "Boss.Phase.ForceNext" → 次フェーズへ
	// 具体的なタグ運用はサブクラスまたはBossCharacterで定義する
	void HandleAttackEvent(FGameplayTag Tag);

	// UPartDestructionComponent::OnPartDestroyedにバインド
	void HandlePartDestroyed(FName PartTag);

private:

	// HP変化コールバック。StatusComponent::OnHPChangedにバインド
	UFUNCTION()
	void OnHPChanged(float NewHP, float MaxHP);

	void TransitionToPhase(int32 NewPhaseIndex);

	UPROPERTY()
	TObjectPtr<const UBossDataAsset> Data;

	int32 CurrentPhaseIndex = 0;

};
