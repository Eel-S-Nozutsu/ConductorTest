// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_PreferredRange.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_PreferredRange::UEnvQueryTest_PreferredRange()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_PreferredRange::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData());
	if (!Data) return;

	const float Preferred = FMath::Max(Data->AISettings.PreferredRange, 1.0f);

	// コンテキスト(TargetActorの位置)を解決
	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) ||
		TargetLocations.IsEmpty()) return;

	const FVector TargetLoc = TargetLocations[0];

	// 各グリッド点をスコアリング
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const float Dist = FVector::Dist2D(ItemLoc, TargetLoc);
		const float Score = FMath::Max(0.0f, 1.0f - FMath::Abs(Dist - Preferred) / Preferred);
		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);
	}
}

FText UEnvQueryTest_PreferredRange::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Preferred Range"));
}

FText UEnvQueryTest_PreferredRange::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("DataAsset.PreferredRangeまでの距離でスコアを算出します"));
}
