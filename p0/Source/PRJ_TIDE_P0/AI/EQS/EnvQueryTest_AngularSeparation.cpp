// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_AngularSeparation.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"

#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_AngularSeparation::UEnvQueryTest_AngularSeparation()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_AngularSeparation::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	UWorld* World = Enemy->GetWorld();
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) || TargetLocations.IsEmpty()) return;

	const FVector TargetLoc = TargetLocations[0];

	// 他の敵がPCから見てどの方位にいるか
	TArray<FVector> AnchorDirs;
	for (const FVector& Anchor : Director->GetLaneAnchors(Enemy))
	{
		const FVector Dir = (Anchor - TargetLoc).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) continue;

		AnchorDirs.Add(Dir);

		if (bDrawDebug)
		{
			DrawDebugLine(World, TargetLoc, Anchor, FColor::Orange, false, DebugDuration, 0, 2.0f);
		}
	}

	// 他に敵がいなければ全候補が満点 ※SetScoreを回さず抜ける
	if (AnchorDirs.IsEmpty()) return;

	const float MinSeparation = FMath::Max(MinSeparationDeg, KINDA_SMALL_NUMBER);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const FVector ItemDir = (ItemLoc - TargetLoc).GetSafeNormal2D();

		// PCの真上に重なる候補は方位が定まらない ※最低点にして避けさせる
		float Score = ItemDir.IsNearlyZero() ? 0.0f : 1.0f;

		if (!ItemDir.IsNearlyZero())
		{
			// 最も方位が近い敵との角度差でスコアが決まる
			float ClosestDeg = 180.0f;
			for (const FVector& AnchorDir : AnchorDirs)
			{
				const float Dot = FMath::Clamp(FVector::DotProduct(ItemDir, AnchorDir), -1.0f, 1.0f);
				ClosestDeg = FMath::Min(ClosestDeg, FMath::RadiansToDegrees(FMath::Acos(Dot)));
			}
			Score = FMath::Clamp(ClosestDeg / MinSeparation, 0.0f, 1.0f);
		}

		It.SetScore(TestPurpose, FilterType, Score, 0.0f, 1.0f);

		if (bDrawDebug)
		{
			const FColor Color = FColor(255 - static_cast<uint8>(Score * 255.0f), static_cast<uint8>(Score * 255.0f), 0);
			DrawDebugPoint(World, ItemLoc, 12.0f, Color, false, DebugDuration);
		}
	}
}

FText UEnvQueryTest_AngularSeparation::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Angular Separation"));
}

FText UEnvQueryTest_AngularSeparation::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("他の敵とPCから見た方位角が近い点を減点"));
}
