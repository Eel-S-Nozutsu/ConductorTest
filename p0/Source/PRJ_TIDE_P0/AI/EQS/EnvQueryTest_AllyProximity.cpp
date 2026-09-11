// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_AllyProximity.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"

#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_AllyProximity::UEnvQueryTest_AllyProximity()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_AllyProximity::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	UWorld* World = Enemy->GetWorld();
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	const TArray<FVector> AllyLocations = Director->GetCombatantLocations(Enemy);

	// 他に敵がいなければ全候補が満点 ※SetScoreを回さず抜ける
	if (AllyLocations.IsEmpty()) return;

	if (bDrawDebug)
	{
		for (const FVector& Loc : AllyLocations)
		{
			DrawDebugCircle(World, Loc, ProximityRadius, 24, FColor::Orange, false, DebugDuration, 0, 2.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	}

	const float Radius = FMath::Max(ProximityRadius, KINDA_SMALL_NUMBER);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());

		// 最も近い敵との距離でスコアが決まる ※半径外なら満点
		float ClosestDist = Radius;
		for (const FVector& Loc : AllyLocations)
		{
			ClosestDist = FMath::Min(ClosestDist, FVector::Dist2D(ItemLoc, Loc));
		}

		const float Score = FMath::Lerp(MinScore, 1.0f, FMath::Clamp(ClosestDist / Radius, 0.0f, 1.0f));
		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);

		if (bDrawDebug)
		{
			const FColor Color = FColor(255 - static_cast<uint8>(Score * 255.0f), static_cast<uint8>(Score * 255.0f), 0);
			DrawDebugPoint(World, ItemLoc, 12.0f, Color, false, DebugDuration);
		}
	}
}

FText UEnvQueryTest_AllyProximity::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Ally Proximity"));
}

FText UEnvQueryTest_AllyProximity::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("他の敵の現在位置に近い点を減点"));
}
