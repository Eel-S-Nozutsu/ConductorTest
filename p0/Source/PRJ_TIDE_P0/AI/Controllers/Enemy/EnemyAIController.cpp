// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyPerceptionComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

AEnemyAIController::AEnemyAIController(const FObjectInitializer& ObjectInitializer)
	// DetourCrowdはCrowdManagerの全体MaxAgents上限に縛られ、
	// 大量出現で超過分が動かなくなるため使わない。標準のPathFollowingComponentを使う
	: Super(ObjectInitializer)
{
	EnemyPerception = CreateDefaultSubobject<UEnemyPerceptionComponent>(
		TEXT("EnemyPerception"));
	ThreatComponent = CreateDefaultSubobject<UEnemyThreatComponent>(
		TEXT("ThreatComponent"));
	BrainStateMachine = CreateDefaultSubobject<UEnemyBrainComponent>(
		TEXT("BrainStateMachine"));
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BrainStateMachine)
	{
		BrainStateMachine->StartLogic();
	}

	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(InPawn))
	{
		if (const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData()))
		{
			// -1で無効。有効ならDAのSightRadiusを上書きする
			const float SightRadius = Enemy->GetSightRadiusOverride() >= 0.0f
				? Enemy->GetSightRadiusOverride()
				: Data->Perception.SightRadius;

			EnemyPerception->ApplySightSettings(
				SightRadius,
				Data->Perception.PeripheralVisionAngle);

			if (ThreatComponent)
			{
				ThreatComponent->ApplyDetectionSettings(
					Data->Perception.DetectionFillSeconds,
					Data->Perception.DetectionDecaySeconds);
			}
		}
	}
}

void AEnemyAIController::HandleEncounterActivated()
{
	if (ThreatComponent)
	{
		ThreatComponent->SetEngaged(true);
	}
}

void AEnemyAIController::HandleEncounterDeactivated()
{
	if (ThreatComponent)
	{
		ThreatComponent->SetEngaged(false);
	}
}
