// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Volume/BossJumpTrigger.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/BossCharacter.h"

#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ABossJumpTrigger::ABossJumpTrigger()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABossJumpTrigger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (!bDrawDebug) return;

	// ボリューム範囲: 未通過=緑/通過済み=灰
	const FColor BoxColor = (bConsumeOnce && bConsumed) ? FColor(110, 110, 110) : FColor::Green;
	FVector Origin, Extent;
	GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
	DrawDebugBox(GetWorld(), Origin, Extent, BoxColor, false, -1.0f, 0, 2.0f);

	// ターゲット足場への矢印
	if (TargetPlatform)
	{
		const FVector To = TargetPlatform->GetActorLocation();
		DrawDebugDirectionalArrow(GetWorld(), Origin, To, 120.0f, FColor::Cyan, false, -1.0f, 0, 3.0f);
		DrawDebugSphere(GetWorld(), To, 40.0f, 12, FColor::Cyan, false, -1.0f, 0, 2.0f);
	}
	else
	{
		// ターゲット未設定は赤で警告
		DrawDebugString(GetWorld(), Origin, TEXT("TargetPlatform 未設定"), nullptr, FColor::Red, 0.0f, true);
	}
#endif
}

void ABossJumpTrigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (bConsumeOnce && bConsumed) return;
	if (!Cast<ATidePlayerCharacter>(OtherActor)) return;
	if (!TargetPlatform) return;

	// ボス未設定ならレベルから自動取得 (テストルームはボス1体)
	if (!Boss)
	{
		Boss = Cast<ABossCharacter>(
			UGameplayStatics::GetActorOfClass(this, ABossCharacter::StaticClass()));
	}
	if (!Boss) return;

	Boss->EnqueuePlatformJump(TargetPlatform->GetActorLocation());
	bConsumed = true;
}
