// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_SlotOccupancy.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"

#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_SlotOccupancy::UEnvQueryTest_SlotOccupancy()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_SlotOccupancy::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UWorld* World = QueryInstance.World.Get();
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	AEnemyCharacter* QuerierEnemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);

	if (bDrawDebug)
	{
		for (const TPair<AEnemyCharacter*, FVector>& Slot : Director->GetDesiredPositions())
		{
			if (Slot.Key == QuerierEnemy) continue;
			DrawDebugCircle(World, Slot.Value, OccupancyRadius, 24, FColor::Cyan, false, DebugDuration, 0, 2.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		const float Score = Director->GetOccupancyScore(ItemLocation, QuerierEnemy, OccupancyRadius);

		// Filter時はOccupancyRadius内(=Score<1.0)を除外する
		// 半径外はちょうど1.0なのでMinimum/1.0で切れる
		// FilterMinに0.0を渡すと0.0>=0.0で全通過するため1.0固定
		// Score時はこの判定はスキップされ、勾配スコアがそのまま使われる
		It.SetScore(TestPurpose, EEnvTestFilterType::Minimum, Score, 1.0f, 1.0f);

		if (bDrawDebug)
		{
			const FColor Color = FColor(255 - static_cast<uint8>(Score * 255.0f), static_cast<uint8>(Score * 255.0f), 0);
			DrawDebugPoint(World, ItemLocation, 12.0f, Color, false, DebugDuration);
		}
	}
}

FText UEnvQueryTest_SlotOccupancy::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Slot Occupancy"));
}

FText UEnvQueryTest_SlotOccupancy::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("AIDirectorの登録済みDesiredPositionに近い点を減点"));
}
