// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_DistanceRange.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_DistanceRange::UEnvQueryTest_DistanceRange()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Range;
}

void UEnvQueryTest_DistanceRange::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData());
	if (!Data) return;

	const float MinRange = Data->AISettings.MinEngageRange;
	const float MaxRange = Data->AISettings.MaxEngageRange;

	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) || TargetLocations.IsEmpty()) return;

	const FVector TargetLoc = TargetLocations[0];

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const float Dist = FVector::Dist2D(ItemLoc, TargetLoc);

		// ビルトインDistanceテストと同じパターン第4・5引数がフィルター閾値
		// (FilterType=Range: MinRange <= Dist <=
		// MaxRangeで通過)Score時はMinRange〜MaxRange内を0.0〜1.0で正規化す
		// るためScoreでも壊れない
		It.SetScore(TestPurpose, FilterType, Dist, MinRange, MaxRange);
	}
}

FText UEnvQueryTest_DistanceRange::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Distance Range"));
}

FText UEnvQueryTest_DistanceRange::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("DataAsset.MinEngageRange〜MaxEngageRangeで足切り"));
}
