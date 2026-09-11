// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GenericTeamAgentInterface.h"
#include "EnemyAIController.generated.h"

/**
 *
 */
UCLASS()
class PRJ_TIDE_P0_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:

	AEnemyAIController(const FObjectInitializer& ObjectInitializer);

	void OnPossess(APawn* InPawn) override;

	// スポナーのOnSpawnerActivated/OnSpawnerDeactivatedへ購読するハ
	// ンドラ検知の有効/無効を切り替える ※スポナーが知覚コンポーネントを直接触らないための境界
	void HandleEncounterActivated();
	void HandleEncounterDeactivated();

	// チームID: 1 = Enemy
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(1); }

	// ストレイフの移動先を選ぶEQSクエリ (EQS_Enemy)
	UPROPERTY(EditDefaultsOnly, Category = "Tide|AI")
	TObjectPtr<class UEnvQuery> StrafeQuery = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Tide|AI")
	TObjectPtr<class UEnemyPerceptionComponent> EnemyPerception = nullptr;

	// ターゲット選定・記憶 + エンカウント状態 ※TargetActorの唯一の書き手
	UPROPERTY(VisibleDefaultsOnly, Category = "Tide|AI")
	TObjectPtr<class UEnemyThreatComponent> ThreatComponent = nullptr;

	// 敵AIのフロー制御 (旧BehaviorTree)
	UPROPERTY(VisibleDefaultsOnly, Category = "Tide|AI")
	TObjectPtr<class UEnemyBrainComponent> BrainStateMachine = nullptr;

};
