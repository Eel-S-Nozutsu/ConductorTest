// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_ReturnToOrigin.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

void UEnemyAIState_ReturnToOrigin::Enter(UEnemyBrainComponent& Brain)
{
	Brain.ApplyMoveSpeed(EEnemySpeedType::Run);

	AEnemyAIController* AIController = Brain.GetAIController();
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!AIController || !Enemy) return;

	// TargetActorの書き手はThreatなので、クリアもThreat経由で行う
	if (AIController->ThreatComponent)
	{
		AIController->ThreatComponent->ClearTarget();
	}
	AIController->ClearFocus(EAIFocusPriority::Gameplay);

	if (UAIDirector* Director = Enemy->GetWorld()->GetSubsystem<UAIDirector>())
	{
		Director->UnregisterDesiredPosition(Enemy);
	}

	Enemy->SetReturningHome(true);
	Enemy->AddStateTag(TAG_State_Common_SuperArmor_Invincible);

	RetryAccum       = 0.0f;
	StuckStrikes     = 0;
	LastDistToOrigin = -1.0f;

	// 初回は素直に中心を狙う (真ん中へ戻る絵)。詰まったら再試行でランダム候補に切り替える
	AttemptReturnMove(Brain, /*bRandom=*/false);
}

bool UEnemyAIState_ReturnToOrigin::ComputeReturnGoal(UEnemyBrainComponent& Brain, bool bRandom, FVector& OutGoal) const
{
	const float PatrolRadius = Brain.GetPatrolRadius();
	if (PatrolRadius <= 0.0f) return false;

	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return false;

	const FVector PatrolOrigin = Brain.GetPatrolOrigin();

	// テリトリー円内から一様に1点サンプルする (中心密集を避けるため半径はsqrt補正)
	FVector Candidate = PatrolOrigin;
	if (bRandom)
	{
		const float R     = FMath::Sqrt(FMath::FRand()) * PatrolRadius * SampleRadiusRatio;
		const float Theta = FMath::FRandRange(0.0f, 2.0f * PI);
		Candidate += FVector(R * FMath::Cos(Theta), R * FMath::Sin(Theta), 0.0f);
	}

	// 射影前の候補を返しておく (失敗時のデバッグ線用)
	OutGoal = Candidate;

	// スポナー中央/候補はアクターの設置高さによってナビメッシュから浮いていることがある
	// そのままだとMoveToがゴール対応ポリを見つけられず即失敗するため、ナビ上の点へ射影する
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld()))
	{
		FNavLocation Projected;
		const FVector ProjectExtent(PatrolRadius, PatrolRadius, PatrolRadius);
		if (!NavSys->ProjectPointToNavigation(Candidate, Projected, ProjectExtent))
		{
			// ナビ上に帰還先が見つからない: 戻りようがない
			return false;
		}
		OutGoal = Projected.Location;
	}
	return true;
}

void UEnemyAIState_ReturnToOrigin::AttemptReturnMove(UEnemyBrainComponent& Brain, bool bRandom)
{
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return;

	FVector Goal = Brain.GetPatrolOrigin();
	const bool bHasGoal = ComputeReturnGoal(Brain, bRandom, Goal);
	const bool bSuccess = bHasGoal && Brain.RequestMoveTo(Goal, AcceptanceRadius);

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawReturnHome)
	{
		// パス検索の成否を可視化: 成功=緑 / 失敗=赤。候補点に球、敵から候補へ線を引く
		const FColor Color = bSuccess ? FColor::Green : FColor::Red;
		DrawDebugSphere(Enemy->GetWorld(), Goal, 40.0f, 12, Color, false, RetryInterval, 0, 2.0f);
		DrawDebugLine(Enemy->GetWorld(), Enemy->GetActorLocation(), Goal, Color, false, RetryInterval, 0, 3.0f);
	}
#endif
}

void UEnemyAIState_ReturnToOrigin::WarpHome(UEnemyBrainComponent& Brain)
{
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return;

	// 射影に成功すればナビ上の点、ダメなら原点そのまま。カプセル半分持ち上げて床めり込みを防ぐ
	FVector Goal = Brain.GetPatrolOrigin();
	ComputeReturnGoal(Brain, /*bRandom=*/false, Goal);
	if (const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent())
	{
		Goal.Z += Capsule->GetScaledCapsuleHalfHeight();
	}

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugDrawReturnHome)
	{
		// ワープ発火: 元位置→ワープ先を青線、両端に球で残す
		DrawDebugLine(Enemy->GetWorld(), Enemy->GetActorLocation(), Goal, FColor::Cyan, false, 2.0f, 0, 4.0f);
		DrawDebugSphere(Enemy->GetWorld(), Goal, 60.0f, 16, FColor::Cyan, false, 2.0f, 0, 3.0f);
	}
#endif

	Brain.StopMovement();
	Enemy->SetActorLocation(Goal, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = Enemy->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}

	// ワープ後は圏内。次のブレイン評価がReturnToOriginから自然に抜ける
	StuckStrikes     = 0;
	LastDistToOrigin = -1.0f;
}

void UEnemyAIState_ReturnToOrigin::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	// 帰還中も現在の検知状態からTargetActorを再導出する
	// PCが視界に入りっぱなしだとAIPerceptionの新規感知イベントが出ずTargetActorが
	// 空のままになり、テリトリー内へ戻っても再交戦できないため、ここで毎フレーム補う
	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		if (AIController->ThreatComponent)
		{
			AIController->ThreatComponent->RefreshTargetActor();
		}
	}

	// EnterのMoveToが即失敗/中断で終わっても帰還ステートからは自力で抜けられない
	// (優先度評価は圏外の間ReturnToOriginを返し続ける)。無敵のまま棒立ちを避けるため、
	// 原点への進捗を周期監視し、詰まっていたら別候補で張り直し、それでもダメならワープする
	RetryAccum += DeltaSeconds;
	if (RetryAccum < RetryInterval) return;
	RetryAccum = 0.0f;

	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!Enemy) return;

	const float CurDist  = FVector::Dist2D(Enemy->GetActorLocation(), Brain.GetPatrolOrigin());
	const bool  bProgress = (LastDistToOrigin < 0.0f) || (CurDist < LastDistToOrigin - ProgressEpsilon);
	LastDistToOrigin = CurDist;

	// 近づけていて移動も生きている = 順調。カウンタを戻して見守る
	if (bProgress && Brain.IsMoveInProgress())
	{
		StuckStrikes = 0;
		return;
	}

	// idle or 壁ずりで停滞。規定回数を超えたらワープ、まだなら別のランダム候補で再試行
	if (++StuckStrikes >= MaxRetries)
	{
		WarpHome(Brain);
		return;
	}
	AttemptReturnMove(Brain, /*bRandom=*/true);
}

void UEnemyAIState_ReturnToOrigin::Exit(UEnemyBrainComponent& Brain)
{
	Brain.StopMovement();

	// 付与していなくても解除は無害
	if (AEnemyCharacter* Enemy = Brain.GetEnemy())
	{
		Enemy->SetReturningHome(false);
		Enemy->RemoveStateTag(TAG_State_Common_SuperArmor_Invincible);
	}

	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		if (AIController->ThreatComponent)
		{
			AIController->ThreatComponent->RefreshTargetActor();
		}
	}
}
