// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_NotCrossingAlly.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"

#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

namespace
{
	// PCから見た方位角 ※PC上に重なると方位が定まらず失敗を返す
	bool GetBearingDeg(const FVector& From, const FVector& Origin, float& OutDeg)
	{
		const FVector Dir = (From - Origin).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) return false;

		OutDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		return true;
	}
}

UEnvQueryTest_NotCrossingAlly::UEnvQueryTest_NotCrossingAlly()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
}

void UEnvQueryTest_NotCrossingAlly::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	UWorld* World = Enemy->GetWorld();
	UAIDirector* Director = World ? World->GetSubsystem<UAIDirector>() : nullptr;
	if (!Director) return;

	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) || TargetLocations.IsEmpty()) return;

	const FVector TargetLoc = TargetLocations[0];

	float SelfDeg = 0.0f;
	if (!GetBearingDeg(Enemy->GetActorLocation(), TargetLoc, SelfDeg)) return;

	// 自分から見た他の敵の相対方位 ※符号=回る向き、絶対値=回る量
	TArray<float> AnchorDeltas;
	for (const FVector& Anchor : Director->GetLaneAnchors(Enemy))
	{
		float AnchorDeg = 0.0f;
		if (!GetBearingDeg(Anchor, TargetLoc, AnchorDeg)) continue;

		AnchorDeltas.Add(FMath::FindDeltaAngleDegrees(SelfDeg, AnchorDeg));
	}

	if (AnchorDeltas.IsEmpty()) return;

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());

		float ItemDeg = 0.0f;
		if (!GetBearingDeg(ItemLoc, TargetLoc, ItemDeg))
		{
			It.SetScore(TestPurpose, FilterType, CrossingScore, 0.0f, 1.0f);
			continue;
		}

		const float ItemDelta = FMath::FindDeltaAngleDegrees(SelfDeg, ItemDeg);

		// 回る向きが同じで、かつ自分より手前にいる敵は通過することになる
		bool bCrosses = false;
		for (const float AnchorDelta : AnchorDeltas)
		{
			const bool bSameDirection = (AnchorDelta * ItemDelta) > 0.0f;
			if (bSameDirection && FMath::Abs(AnchorDelta) <= FMath::Abs(ItemDelta))
			{
				bCrosses = true;
				break;
			}
		}

		It.SetScore(TestPurpose, FilterType, bCrosses ? CrossingScore : 1.0f, 0.0f, 1.0f);

		if (bDrawDebug && bCrosses)
		{
			DrawDebugLine(World, Enemy->GetActorLocation(), ItemLoc, FColor::Red, false, DebugDuration, 0, 1.2f);
		}
	}
}

FText UEnvQueryTest_NotCrossingAlly::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Not Crossing Ally"));
}

FText UEnvQueryTest_NotCrossingAlly::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("移動中に他の敵の前を横切る点を減点"));
}
