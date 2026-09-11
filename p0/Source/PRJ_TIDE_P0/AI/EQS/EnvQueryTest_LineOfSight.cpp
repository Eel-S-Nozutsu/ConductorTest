// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_LineOfSight.h"
#include "PRJ_TIDE_P0/AI/EQS/EQSTestUtils.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEnvQueryTest_LineOfSight::UEnvQueryTest_LineOfSight()
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Filter;
	FilterType = EEnvTestFilterType::Minimum;
}

void UEnvQueryTest_LineOfSight::RunTest(FEnvQueryInstance& QueryInstance) const
{
	AEnemyCharacter* Enemy = EQSTestUtils::ResolveQuerierEnemy(QueryInstance);
	if (!Enemy) return;

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData());
	if (!Data) return;

	const float HeightOffset = Data->AISettings.EyeHeightOffset;

	TArray<AActor*> TargetActors;
	if (!QueryInstance.PrepareContext(TargetContext, TargetActors) || TargetActors.IsEmpty()) return;

	AActor* Target = TargetActors[0];
	if (!Target) return;

	const FVector TraceEnd = Target->GetActorLocation();

	UWorld* World = Enemy->GetWorld();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Enemy);
	Params.AddIgnoredActor(Target);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());
		const FVector TraceStart = ItemLoc + FVector(0.0f, 0.0f, HeightOffset);

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

		It.SetScore(TestPurpose, FilterType, bBlocked ? 0.0f : 1.0f, 0.0f, 1.0f);
	}
}

FText UEnvQueryTest_LineOfSight::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Line Of Sight"));
}

FText UEnvQueryTest_LineOfSight::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("移動先+EyeHeightOffsetからターゲットへ視線トレース"));
}
