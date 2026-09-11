// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_PreferredMoveDistance.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_PreferredMoveDistance::UEnvQueryTest_PreferredMoveDistance()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_PreferredMoveDistance::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData());
	if (!Data) return;

	const float Preferred = FMath::Max(Data->AISettings.PreferredMoveDistance, 1.0f);
	const FVector EnemyLoc = Enemy->GetActorLocation();

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const float Dist = FVector::Dist2D(ItemLoc, EnemyLoc);

		// Preferredに近いほど1.0、0やPreferred*2で0.0になる山型スコア
		const float Score = FMath::Max(0.0f, 1.0f - FMath::Abs(Dist - Preferred) / Preferred);

		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);
	}
}

FText UEnvQueryTest_PreferredMoveDistance::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Preferred Move Distance"));
}

FText UEnvQueryTest_PreferredMoveDistance::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("DataAsset.PreferredMoveDistanceに近い移動量の点を高スコアにする"));
}
