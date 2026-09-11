// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_NotBehindTarget.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"

#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_NotBehindTarget::UEnvQueryTest_NotBehindTarget()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Minimum;
}

void UEnvQueryTest_NotBehindTarget::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	TArray<FVector> TargetLocations;
	if (!QueryInstance.PrepareContext(TargetContext, TargetLocations) || TargetLocations.IsEmpty()) return;

	const FVector EnemyLoc = Enemy->GetActorLocation();
	const FVector TargetLoc = TargetLocations[0];
	const FVector DirToTarget = (TargetLoc - EnemyLoc).GetSafeNormal2D();
	const float DistToTarget = FVector::Dist2D(EnemyLoc, TargetLoc);
	UWorld* World = Enemy->GetWorld();

	if (bDrawDebug && World)
	{
		DrawDebugSphere(World, TargetLoc, MarginRadius, 12, FColor::Cyan, false, DebugDuration, 0, 1.5f);
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const FVector DirToItem = (ItemLoc - EnemyLoc).GetSafeNormal2D();
		const float DistToItem = FVector::Dist2D(EnemyLoc, ItemLoc);

		const float Dot = FVector::DotProduct(DirToTarget, DirToItem);

		// 同方向コーン内 かつ ターゲット付近〜以遠の点 → PCを通り抜けないと到達不能
		const bool bBehindTarget = (Dot > DotThreshold) && (DistToItem >= DistToTarget - MarginRadius);

		// Score(1.0=通過可 / 0.0=除外)
		// を閾値0.5でフィルターFilterType=Minimumで0.0 < 0.5 → 除外、
		// 1.0 >= 0.5 →
		// 通過Score時は0.0〜1.0でスコアリングされるためScore/FilterAndScoreでも
		// 壊れない
		It.SetScore(TestPurpose, FilterType, bBehindTarget ? 0.0f : 1.0f, 0.5f, 1.0f);

		if (bDrawDebug && World)
		{
			const FColor LineColor = bBehindTarget ? FColor::Red : FColor::Green;
			DrawDebugLine(World, TargetLoc, ItemLoc, LineColor, false, DebugDuration, 0, 1.2f);
		}
	}
}

FText UEnvQueryTest_NotBehindTarget::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Not Behind Target"));
}

FText UEnvQueryTest_NotBehindTarget::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("ターゲットの向こう側への移動を除外"));
}
