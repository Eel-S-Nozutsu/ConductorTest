// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ConductorTriggerComponent.generated.h"

class UPrimitiveComponent;

/**
 * 接触したらコンテンツコンダクタへイベントを送るだけの箱
 * オーナーのコリジョンに自分で繋ぐので、BPを書かずにレベル上で完結する
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class CONDUCTORTEST_API UConductorTriggerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UConductorTriggerComponent();

	void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Conductor", meta = (DisplayName = "対象のコンテンツId"))
	FName ContentId;

	UPROPERTY(EditAnywhere, Category = "Conductor", meta = (DisplayName = "送るイベント"))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, Category = "Conductor", meta = (DisplayName = "反応する相手 (空なら全部)"))
	TSubclassOf<AActor> TargetClass;

	UPROPERTY(EditAnywhere, Category = "Conductor", meta = (DisplayName = "一度だけかどうか"))
	bool bOnce = true;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bFired = false;
};
