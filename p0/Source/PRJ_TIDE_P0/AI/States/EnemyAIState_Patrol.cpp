// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Patrol.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Patrol/EnemyRoutePath.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#if !UE_BUILD_SHIPPING
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "DrawDebugHelpers.h"
#endif

void UEnemyAIState_Patrol::Enter(UEnemyBrainComponent& Brain)
{
	Brain.ApplyMoveSpeed(EEnemySpeedType::Walk);

	// 徘徊中は注視しない (ターゲットがいないので向く先もない)
	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	bWaiting = false;
	WaitRemaining = 0.0f;

	if (!PickNextDestination(Brain))
	{
		bWaiting = true;
		WaitRemaining = IsRouteMode(Brain) ? RoutePollInterval : RetryInterval;
	}
}

void UEnemyAIState_Patrol::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	DrawRouteDebug(Brain);
#endif

	if (bWaiting)
	{
		WaitRemaining -= DeltaSeconds;
		if (WaitRemaining > 0.0f) return;

		bWaiting = false;
		if (!PickNextDestination(Brain))
		{
			bWaiting = true;
			WaitRemaining = IsRouteMode(Brain) ? RoutePollInterval : RetryInterval;
		}
		return;
	}

	// 移動が終わった = 到着 (または経路断念)。到達判定はここ1箇所だけが持つ
	if (Brain.IsMoveInProgress()) return;

	if (IsRouteMode(Brain))
	{
		OnReachedRoutePoint(Brain);
		return;
	}

	bWaiting = true;
	WaitRemaining = WaitDuration;
}

void UEnemyAIState_Patrol::Exit(UEnemyBrainComponent& Brain)
{
	Brain.StopMovement();

	// ターゲットを見つけて戦闘へ移る等。終着待機モーションは畳む
	SetTerminalIdleActive(Brain, false);
}

bool UEnemyAIState_Patrol::IsRouteMode(const UEnemyBrainComponent& Brain) const
{
	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return false;

	const FEnemyWaitSettings& Settings = Enemy->GetWaitSettings();
	return Settings.WaitType == EEnemyWaitType::RouteMove && IsValid(Settings.RoutePath.Get());
}

bool UEnemyAIState_Patrol::PickNextDestination(UEnemyBrainComponent& Brain)
{
	// ルートを持つ敵はランダム徘徊へ流さない (経路を無視して歩き出すため)
	return IsRouteMode(Brain) ? AdvanceRoute(Brain) : PickRandomDestination(Brain);
}

// ------------------------------------------------------------
// ランダム徘徊
// ------------------------------------------------------------

bool UEnemyAIState_Patrol::PickRandomDestination(UEnemyBrainComponent& Brain)
{
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
	if (!NavSys) return false;

	// 徘徊先は常に自身の周囲から選ぶ。テリトリー全体を探索範囲にすると
	// 1回の徘徊でスポナー領域の対角線まで走ってしまう
	// (テリトリーは行動範囲の上限であって歩幅ではない)
	const FVector SelfLocation = Enemy->GetActorLocation();
	const FVector PatrolOrigin = Brain.GetPatrolOrigin();
	const float PatrolRadius = Brain.GetPatrolRadius();

	FNavLocation NextLocation;
	bool bWithinTerritory = false;
	for (int32 Attempt = 0; Attempt < MaxWanderAttempts && !bWithinTerritory; ++Attempt)
	{
		if (!NavSys->GetRandomReachablePointInRadius(SelfLocation, WanderRadius, NextLocation))
		{
			return false;
		}

		// テリトリー未設定 (スポナー非所属) なら制約なし
		bWithinTerritory = PatrolRadius <= 0.0f
			|| FVector::Dist2D(NextLocation.Location, PatrolOrigin) <= PatrolRadius;
	}

	// 境界際にいて外側ばかり引いた場合。テリトリー円内へ引き戻す
	if (!bWithinTerritory)
	{
		FVector FromOrigin = NextLocation.Location - PatrolOrigin;
		FromOrigin.Z = 0.0f;
		const float DistFromOrigin = FromOrigin.Size();
		if (DistFromOrigin <= KINDA_SMALL_NUMBER) return false;

		FVector Clamped = PatrolOrigin + FromOrigin / DistFromOrigin * PatrolRadius;
		Clamped.Z = SelfLocation.Z;

		FNavLocation Projected;
		if (!NavSys->ProjectPointToNavigation(Clamped, Projected, FVector(300.0f, 300.0f, 1000.0f)))
		{
			return false;
		}
		NextLocation = Projected;
	}

	return Brain.RequestMoveTo(NextLocation.Location);
}

// ------------------------------------------------------------
// ルート移動
// ------------------------------------------------------------

bool UEnemyAIState_Patrol::AdvanceRoute(UEnemyBrainComponent& Brain)
{
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	const AEnemyRoutePath* RoutePath = Enemy ? Enemy->GetWaitSettings().RoutePath.Get() : nullptr;
	if (!RoutePath) return false;

	const int32 PointCount = RoutePath->GetRoutePointCount();
	if (PointCount <= 0) return false;

	// RouteAcceptanceRadiusは「どこまで近づけば到達扱いか」
	// 到達判定はMoveToの完了そのものなので、この値がそのまま停止距離になる
	const float AcceptRadius = Enemy->GetWaitSettings().RouteAcceptanceRadius;

	// 初回は一番近いポイントから始める
	if (!bRouteInitialized)
	{
		bRouteInitialized = true;
		RouteCurrentPointIndex = RoutePath->FindNearestRoutePointIndex(Enemy->GetActorLocation());
		RouteTargetPointIndex = INDEX_NONE;
		RouteDirection = 1;
		bRouteTerminalReached = false;

		if (RouteCurrentPointIndex == INDEX_NONE) return false;

		// 最寄りポイントからまだ離れているなら、まずそこへ向かう
		// 現在地として記録しただけで次点を計算すると、ルートの1点目を飛ばして2点目へ行ってしまう
		FVector NearestLocation = FVector::ZeroVector;
		if (ResolveRoutePointLocation(Brain, RouteCurrentPointIndex, NearestLocation)
			&& FVector::Dist2D(Enemy->GetActorLocation(), NearestLocation) > AcceptRadius)
		{
			RouteTargetPointIndex = RouteCurrentPointIndex;
			SetTerminalIdleActive(Brain, false);
			return Brain.RequestMoveTo(NearestLocation, AcceptRadius);
		}
	}

	// 終着型で終点に着いている: 以降は待機モーションのまま動かない
	if (bRouteTerminalReached)
	{
		SetTerminalIdleActive(Brain, true);
		return false;
	}

	const int32 NextPointIndex = ComputeNextRoutePointIndex(Brain, PointCount);
	if (NextPointIndex == INDEX_NONE)
	{
		if (Enemy->GetWaitSettings().RouteMoveType == EEnemyRouteMoveType::OneWayStop)
		{
			bRouteTerminalReached = true;
			SetTerminalIdleActive(Brain, true);
		}
		return false;
	}

	FVector MoveTarget = FVector::ZeroVector;
	if (!ResolveRoutePointLocation(Brain, NextPointIndex, MoveTarget)) return false;

	RouteTargetPointIndex = NextPointIndex;
	SetTerminalIdleActive(Brain, false);

	return Brain.RequestMoveTo(MoveTarget, AcceptRadius);
}

void UEnemyAIState_Patrol::OnReachedRoutePoint(UEnemyBrainComponent& Brain)
{
	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return;

	if (RouteTargetPointIndex != INDEX_NONE)
	{
		RouteCurrentPointIndex = RouteTargetPointIndex;
		RouteTargetPointIndex = INDEX_NONE;
	}

	// ポイントごとの待機。0ならそのまま次のティックで次点へ進む
	bWaiting = true;
	WaitRemaining = FMath::Max(Enemy->GetWaitSettings().WaitAtRoutePointTime, 0.0f);
}

bool UEnemyAIState_Patrol::ResolveRoutePointLocation(const UEnemyBrainComponent& Brain,
	int32 PointIndex, FVector& OutLocation) const
{
	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	const AEnemyRoutePath* RoutePath = Enemy ? Enemy->GetWaitSettings().RoutePath.Get() : nullptr;
	if (!RoutePath || PointIndex == INDEX_NONE) return false;

	const FVector RoutePointLocation = RoutePath->GetLocationAtRoutePointIndex(PointIndex);
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld()))
	{
		// スプラインポイントがNavメッシュより高く浮いていても直下の地面へ落とすため、
		// 探索ボックスを下方向へ大きく取る (上方向は控えめにして上階のNavを拾わない)
		// ボックスは中心 +/- 半径の対称形なので、中心を下げて非対称な探索範囲を作る
		const float UpReach   = 1000.0f;
		const float DownReach = 10000.0f;
		const FVector QueryCenter = RoutePointLocation + FVector(0.0f, 0.0f, (UpReach - DownReach) * 0.5f);
		const FVector ProjectExtent(120.0f, 120.0f, (UpReach + DownReach) * 0.5f);

		FNavLocation ProjectedLocation;
		if (NavSys->ProjectPointToNavigation(QueryCenter, ProjectedLocation, ProjectExtent))
		{
			OutLocation = ProjectedLocation.Location;
			return true;
		}
	}

	OutLocation = RoutePointLocation;
	return true;
}

int32 UEnemyAIState_Patrol::ComputeNextRoutePointIndex(const UEnemyBrainComponent& Brain, int32 PointCount)
{
	if (RouteCurrentPointIndex == INDEX_NONE || PointCount <= 1) return INDEX_NONE;

	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return INDEX_NONE;

	switch (Enemy->GetWaitSettings().RouteMoveType)
	{
	case EEnemyRouteMoveType::ForwardLoop:
		return (RouteCurrentPointIndex + 1) % PointCount;

	case EEnemyRouteMoveType::PingPongLoop:
	{
		if (RouteCurrentPointIndex <= 0)
		{
			RouteDirection = 1;
		}
		else if (RouteCurrentPointIndex >= PointCount - 1)
		{
			RouteDirection = -1;
		}

		return FMath::Clamp(RouteCurrentPointIndex + RouteDirection, 0, PointCount - 1);
	}

	case EEnemyRouteMoveType::OneWayStop:
		if (RouteCurrentPointIndex >= PointCount - 1) return INDEX_NONE;
		return RouteCurrentPointIndex + 1;
	}

	return INDEX_NONE;
}

void UEnemyAIState_Patrol::SetTerminalIdleActive(UEnemyBrainComponent& Brain, bool bActive)
{
	if (bTerminalIdleActive == bActive) return;

	AEnemyCharacter* Enemy = Brain.GetEnemy();
	UAnimMontage* Montage = Enemy ? Enemy->GetWaitSettings().TerminalIdleMontage.Get() : nullptr;
	UAnimInstance* AnimInstance = (Enemy && Enemy->GetMesh()) ? Enemy->GetMesh()->GetAnimInstance() : nullptr;

	bTerminalIdleActive = bActive;
	if (!AnimInstance || !Montage) return;

	if (bActive)
	{
		if (!AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Play(Montage);
		}
	}
	else
	{
		AnimInstance->Montage_Stop(0.2f, Montage);
	}
}

#if !UE_BUILD_SHIPPING
void UEnemyAIState_Patrol::DrawRouteDebug(const UEnemyBrainComponent& Brain) const
{
	const UTideGameSettings* DebugSettings = UTideGameSettings::Get();
	if (!DebugSettings || !DebugSettings->bDebugDrawRoutePath) return;
	if (RouteTargetPointIndex == INDEX_NONE) return;

	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return;

	FVector TargetPointLocation = FVector::ZeroVector;
	if (!ResolveRoutePointLocation(Brain, RouteTargetPointIndex, TargetPointLocation)) return;

	const FVector Origin = Enemy->GetActorLocation();
	const float Dist2D = FVector::Dist2D(Origin, TargetPointLocation);
	const float Accept = Enemy->GetWaitSettings().RouteAcceptanceRadius;
	const FColor LineColor = (Dist2D <= Accept) ? FColor::Green : FColor::Cyan;
	UWorld* World = Enemy->GetWorld();

	DrawDebugLine(World, Origin, TargetPointLocation, LineColor, false, 0.0f, 0, 2.0f);
	DrawDebugSphere(World, TargetPointLocation, 20.0f, 12, LineColor, false, 0.0f, 0, 1.5f);

	const FVector CylTop = TargetPointLocation + FVector(0.0f, 0.0f, 120.0f);
	DrawDebugCylinder(World, TargetPointLocation, CylTop, Accept, 24, FColor::Yellow, false, 0.0f, 0, 1.0f);

	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	DrawDebugString(World, Origin + FVector(0.0f, 0.0f, HalfHeight + 40.0f),
		FString::Printf(TEXT("Route[%d] %.0f / %.0f"), RouteTargetPointIndex, Dist2D, Accept),
		nullptr, LineColor, 0.0f, true);
}
#endif

FString UEnemyAIState_Patrol::GetDebugText() const
{
	if (bRouteTerminalReached) return TEXT("route terminal");

	if (RouteTargetPointIndex != INDEX_NONE)
	{
		return FString::Printf(TEXT("route -> %d"), RouteTargetPointIndex);
	}

	return bWaiting ? FString::Printf(TEXT("wait %.1fs"), WaitRemaining) : TEXT("move");
}
