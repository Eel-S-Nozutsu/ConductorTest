// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/EQS/EnvQueryTest_ScreenPresence.h"

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/PlayerController.h"

UEnvQueryTest_ScreenPresence::UEnvQueryTest_ScreenPresence()
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);
	TestPurpose = EEnvTestPurpose::Filter;
	// 真偽値テストはMatchで判定する。Minimumだと閾値0.0に対しオフ画面(0.0)も
	// 0.0>=0.0で通過してしまい足切りできない
	FilterType = EEnvTestFilterType::Match;
}

void UEnvQueryTest_ScreenPresence::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UWorld* World = GEngine->GetWorldFromContextObject(QueryInstance.Owner.Get(), EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLoc = GetItemLocation(QueryInstance, It.GetIndex());

		FVector2D ScreenPos;
		const bool bProjected = PC->ProjectWorldLocationToScreen(ItemLoc, ScreenPos);
		const bool bOnScreen = bProjected
			&& ScreenPos.X >= 0.0f && ScreenPos.X <= static_cast<float>(ViewportX)
			&& ScreenPos.Y >= 0.0f && ScreenPos.Y <= static_cast<float>(ViewportY);

		// Filter時はオフ画面を除外、Score時は1.0
		// /0.0になるFilterTypeはMatch固定
		// (memberを使うとeditorでMinimum等にされた時に全滅する)
		It.SetScore(TestPurpose, EEnvTestFilterType::Match, bOnScreen, true);
	}
}

FText UEnvQueryTest_ScreenPresence::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Screen Presence"));
}

FText UEnvQueryTest_ScreenPresence::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("オンスクリーン: 1.0 / オフスクリーン: 0.0"));
}
