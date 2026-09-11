// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_WithinPatrolArea.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"

#include "AIController.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_WithinPatrolArea::UEnvQueryTest_WithinPatrolArea()
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Minimum;
}

void UEnvQueryTest_WithinPatrolArea::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	// 交戦中の移動候補(ストレイフ)を絞るテストなので境界は交戦圏(OuterVolume)。
	// 徘徊圏(Inner)で切るとPCがInner外へ出た瞬間に全候補が落ち、その場で棒立ちになる
	const float PatrolRadius = Enemy->GetCombatRadius();

	// 半径が未設定(0)の場合は全候補を通過させる
	if (PatrolRadius <= 0.0f) return;

	const FVector PatrolOrigin = Enemy->GetPatrolOrigin();

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const float Dist = FVector::Dist2D(ItemLoc, PatrolOrigin);

		// 範囲内なら1.0、範囲外なら0.0 → FilterType=Minimumで0.0の候補を除外
		It.SetScore(TestPurpose, FilterType, Dist <= PatrolRadius ? 1.0f : 0.0f, 1.0f, 1.0f);
	}
}

FText UEnvQueryTest_WithinPatrolArea::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Within Patrol Area"));
}

FText UEnvQueryTest_WithinPatrolArea::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("テリトリーの交戦圏(OuterVolume)外の候補を除外"));
}
