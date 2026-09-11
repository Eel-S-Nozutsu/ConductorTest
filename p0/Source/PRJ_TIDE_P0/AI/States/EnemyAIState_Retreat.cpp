// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Retreat.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "NavigationSystem.h"

void UEnemyAIState_Retreat::Enter(UEnemyBrainComponent& Brain)
{
	Brain.ApplyMoveSpeed(EEnemySpeedType::Run);

	// 離れる方向を向いてダッシュする。開始時に体を離れる方向へ即スナップして、
	// bUseControllerDesiredRotationによるなめらかな振り向きを挟まない
	// PCへの向き直りは攻撃側の向き直りモンタージュに任せる (後退中は背を向けて逃げる)
	if (AActor* Target = Brain.GetTargetActor())
	{
		if (AEnemyCharacter* Enemy = Brain.GetEnemy())
		{
			const FVector Away = (Enemy->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			if (!Away.IsNearlyZero())
			{
				Enemy->SetActorRotation(FRotator(0.0f, Away.Rotation().Yaw, 0.0f));
			}
		}

		// PC注視を外す。残すとdesired-rotationがPC向きへ戻し始めてしまう
		if (AEnemyAIController* AIController = Brain.GetAIController())
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	StartRetreatMove(Brain);
}

void UEnemyAIState_Retreat::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	const AActor* Target = Brain.GetTargetActor();
	if (!Target) return;

	// ターゲットが動けば後退先も変わる。移動が終わっているのに
	// まだIsTooClose (= この状態が続いている) 場合も引き直す
	if (FVector::DistSquared(Target->GetActorLocation(), LastTargetLocation) > FMath::Square(RepathDistance)
		|| !Brain.IsMoveInProgress())
	{
		StartRetreatMove(Brain);
	}
}

void UEnemyAIState_Retreat::Exit(UEnemyBrainComponent& Brain)
{
	Brain.StopMovement();
}

bool UEnemyAIState_Retreat::StartRetreatMove(UEnemyBrainComponent& Brain)
{
	const AEnemyCharacter* Enemy = Brain.GetEnemy();
	const AActor* Target = Brain.GetTargetActor();
	const UEnemyDataAsset* Data = Brain.GetEnemyData();
	if (!Enemy || !Target || !Data) return false;

	// 後退は解除ラインを狙う。ここに届けばIsTooCloseが解除される (判定と同じ値を使う)
	const float RetreatDist = Data->AISettings.GetKeepReleaseDistance();
	if (RetreatDist < 0.0f) return false;

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector EnemyLoc = Enemy->GetActorLocation();
	LastTargetLocation = TargetLoc;

	// PCから見て敵がいる方向 = 離れる向き。真上/同一座標で退化したら敵の後方へフォールバック
	FVector Dir = EnemyLoc - TargetLoc;
	Dir.Z = 0.0f;
	if (!Dir.Normalize())
	{
		Dir = -Enemy->GetActorForwardVector();
		Dir.Z = 0.0f;
		if (!Dir.Normalize()) Dir = FVector::ForwardVector;
	}

	FVector Desired = TargetLoc + Dir * RetreatDist;
	Desired.Z = EnemyLoc.Z;

	// テリトリーの外へ後退しないよう円内にクランプする
	// 交戦中の行動なので境界は交戦圏(OuterVolume)
	const float PatrolRadius = Brain.GetCombatRadius();
	if (PatrolRadius > 0.0f)
	{
		const FVector Origin = Brain.GetPatrolOrigin();
		FVector FromOrigin = Desired - Origin;
		FromOrigin.Z = 0.0f;
		const float DistFromOrigin = FromOrigin.Size();
		if (DistFromOrigin > PatrolRadius)
		{
			Desired = Origin + FromOrigin / DistFromOrigin * PatrolRadius;
			Desired.Z = EnemyLoc.Z;
		}
	}

	// NavMeshへ投影して到達可能な点に補正する。壁際で投影できないときは素の点を使い、
	// 経路探索に委ねる (ここで諦めると逆に接近してしまうため)
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Desired, Projected, FVector(300.0f, 300.0f, 1000.0f)))
		{
			Desired = Projected.Location;
		}
	}

	// 離れる方向を向いて走る。bCanStrafe=falseでパス追従が進行方向へ体を向け続ける
	return Brain.RequestMoveTo(Desired, /*AcceptanceRadius=*/50.0f, /*bCanStrafe=*/false);
}
