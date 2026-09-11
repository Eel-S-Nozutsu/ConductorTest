// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GasCylinderComponent.generated.h"

struct FDamageInfo;

/**
 * ガスボンベ装備品
 * 他の装備品が出てきたタイミングで統合を考える
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRJ_TIDE_P0_API UGasCylinderComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGasCylinderComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UPROPERTY(EditAnywhere, Category = "Tide|Setup")
	TObjectPtr<class UStaticMesh> CylinderMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Tide|VFX")
	TObjectPtr<class UNiagaraSystem> ExplosionEffect = nullptr;

	UPROPERTY(EditAnywhere, Category = "Tide|Setup")
	FName AttachSocketName = TEXT("spine_01");

	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> CylinderMesh = nullptr;

	UFUNCTION()
	void OnDamageReceived(const FDamageInfo& DamageInfo);

	void TriggerExplosion();

	bool IsBackAttack(const FDamageInfo& DamageInfo) const;

};
