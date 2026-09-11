// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "EnemyPerceptionComponent.generated.h"

/**
 * 感知 (Sensing)。「今何が見えるか」だけを担う純粋な知覚コンポーネント。
 * ターゲット選定・記憶・BB書き込みはUEnemyThreatComponentが担当する。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:

	UEnemyPerceptionComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// DataAssetの視野設定を実行時に適用。OnPossessから呼ぶ
	void ApplySightSettings(float SightRadius, float PeripheralVisionAngle);

	// 視野デバッグ表示用の扇マテリアル
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Debug")
	TObjectPtr<UMaterialInterface> DebugFOVMaterial = nullptr;

private:

	void UpdateDebugFOVDecal();

	class UAISenseConfig_Sight* SightConfig = nullptr;
	class UAISenseConfig_Damage* DamageSenseConfig = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<class UDecalComponent> DebugFOVDecal = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DebugFOVMID = nullptr;

};
