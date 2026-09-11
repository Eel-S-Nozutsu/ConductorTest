// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Player/FallRecoveryComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/TimedSteppingStone.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/WaterSurfaceGimmick.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/FragileFloor.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

UFallRecoveryComponent::UFallRecoveryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFallRecoveryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		LastSafeLocation = Owner->GetActorLocation();
	}
}

void UFallRecoveryComponent::TickComponent(float DeltaTime,
	ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryRecordSafeLocation();
}

void UFallRecoveryComponent::TryRecordSafeLocation()
{
	const ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(GetOwner());
	if (!Player || Player->IsFalling()) return;

	// 復帰直後にまた風で落とされてループするため風域内は記録しない
	if (Player->IsInHeadwindZone()) return;

	// 飛び石・水面・崩落床の上は復帰地点として不適切なので記録しない
	if (const UPrimitiveComponent* Base = Player->GetMovementBase())
	{
		AActor* BaseOwner = Base->GetOwner();
		if (Cast<ATimedSteppingStone>(BaseOwner)) return;
		if (Cast<AWaterSurfaceGimmick>(BaseOwner)) return;
		if (Cast<AFragileFloor>(BaseOwner)) return;
	}

	const FVector CurrentLocation = Player->GetActorLocation();
	const float DistSquared = FVector::DistSquared(CurrentLocation, LastSafeLocation);
	if (DistSquared < SafeLocationRecordDistance * SafeLocationRecordDistance) return;

	// 崖際は候補にしない。周囲に地面が続く平坦地のみ記録する
	if (!IsRoomySafeLocation(CurrentLocation)) return;

	LastSafeLocation = CurrentLocation;
}

bool UFallRecoveryComponent::IsRoomySafeLocation(const FVector& Location) const
{
	const ACharacter* Player = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Player || !World) return false;

	// 中心足元を基準高さにし、各サンプル地面がこの高さ付近に揃っているかで平坦性を見る
	const float HalfHeight = Player->GetCapsuleComponent()
		? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const float FeetZ = Location.Z - HalfHeight;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FallRecoveryEdgeMargin), false, Player);

	const int32 NumDirs = FMath::Max(3, EdgeSampleDirections);
	int32 ValidSamples = 0;
	for (int32 i = 0; i < NumDirs; ++i)
	{
		const float Angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(NumDirs);
		const FVector Sample = Location + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * EdgeMarginRadius;

		// サンプル上空から深く下ろす。空なら不通過、段差/崖下に当たっても高さ差で弾かれる
		const FVector TraceStart = FVector(Sample.X, Sample.Y, FeetZ + FlatnessHeightTolerance + 50.0f);
		const FVector TraceEnd = FVector(Sample.X, Sample.Y, FeetZ - 2000.0f);

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

		const bool bFlat = bHit && FMath::Abs(Hit.ImpactPoint.Z - FeetZ) <= FlatnessHeightTolerance;
		const bool bWalkable = bHit && Hit.ImpactNormal.Z >= MinGroundNormalZ;
		const bool bValid = bFlat && bWalkable;
		if (bValid) ++ValidSamples;

		if (bDrawDebugEdgeMargin)
		{
			const FColor Color = bValid ? FColor::Green : FColor::Red;
			DrawDebugLine(World, TraceStart, bHit ? Hit.ImpactPoint : TraceEnd, Color, false, 3.0f, 0, 2.0f);
			if (bHit) DrawDebugPoint(World, Hit.ImpactPoint, 10.0f, Color, false, 3.0f);
		}
	}

	bool bRoomy = ValidSamples >= FMath::Min(MinValidSamples, NumDirs);

	// 補強: 復帰点が実際に立てるNavMesh上にあることを要求する
	if (bRoomy && bRequireOnNavMesh)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation Projected;
			bRoomy = NavSys->ProjectPointToNavigation(Location, Projected, FVector(150.0f, 150.0f, 300.0f));
		}
	}

	if (bDrawDebugEdgeMargin)
	{
		// 候補地点の縁マージン範囲を円柱で表示 (緑=採用/赤=却下)
		const FVector CandBottom(Location.X, Location.Y, FeetZ);
		DrawDebugCylinder(World, CandBottom, CandBottom + FVector(0, 0, 100.0f),
			EdgeMarginRadius, 24, bRoomy ? FColor::Green : FColor::Red, false, 3.0f, 0, 1.5f);
		// 現在採用中の復帰地点も黄円柱で表示
		DrawDebugCylinder(World, LastSafeLocation, LastSafeLocation + FVector(0, 0, 100.0f),
			40.0f, 12, FColor::Yellow, false, 3.0f, 0, 2.0f);
	}

	return bRoomy;
}

void UFallRecoveryComponent::RequestRecovery()
{
	if (bIsRecovering) return;
	bIsRecovering = true;

	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(GetOwner());
	if (!Player)
	{
		bIsRecovering = false;
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeOutDuration,
				FLinearColor::Black, false, true);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this,
		&UFallRecoveryComponent::ExecuteTeleport, FadeOutDuration, false);
}

void UFallRecoveryComponent::ExecuteTeleport()
{
	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(GetOwner());
	if (!Player)
	{
		bIsRecovering = false;
		return;
	}

	Player->SetActorLocation(LastSafeLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	Player->CancelAllActions();

	// 各モジュールのCancelはタグ一致で自分の知るモンタージュしか止めないため、
	// 空中ダイブ攻撃(AIR_ATK/AIRCHARGE_ATK)等が取りこぼされて硬直する
	// ハードなテレポート復帰なので死亡復帰と同様に全モンタージュを無差別停止し、
	// ルートモーション残留も断つ
	Player->StopAnimMontage(0.0f);
	if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
	{
		Movement->RootMotionParams.Clear();
	}

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeInDuration,
				FLinearColor::Black, false, false);
		}
	}

	bIsRecovering = false;
}
