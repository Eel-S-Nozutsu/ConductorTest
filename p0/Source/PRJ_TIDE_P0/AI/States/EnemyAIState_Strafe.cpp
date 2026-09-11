// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Strafe.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "EnvironmentQuery/EnvQueryManager.h"

void UEnemyAIState_Strafe::Enter(UEnemyBrainComponent& Brain)
{
	OwningBrain = &Brain;

	Brain.ApplyMoveSpeed(EEnemySpeedType::Strafe);
	Brain.SetStrafing(true);

	if (AActor* Target = Brain.GetTargetActor())
	{
		if (AEnemyAIController* AIController = Brain.GetAIController())
		{
			AIController->SetFocus(Target);
		}
	}

	StartQuery(Brain);
}

void UEnemyAIState_Strafe::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	switch (Phase)
	{
	case EPhase::Querying:
		// 結果待ち。OnQueryFinishedが次の相へ進める
		break;

	case EPhase::Moving:
		if (!Brain.IsMoveInProgress())
		{
			Phase = EPhase::Waiting;
			WaitRemaining = WaitDuration;
		}
		break;

	case EPhase::Waiting:
		WaitRemaining -= DeltaSeconds;
		if (WaitRemaining <= 0.0f)
		{
			StartQuery(Brain);
		}
		break;
	}
}

void UEnemyAIState_Strafe::Exit(UEnemyBrainComponent& Brain)
{
	// 進行中のクエリ結果を無効化する。EQSは非同期なので抜けた後に返ることがある
	++QueryGeneration;

	Brain.SetStrafing(false);
	Brain.StopMovement();

	// 移動予約を解放する。残したままだと他の敵がこの位置を避け続ける
	if (AEnemyCharacter* Enemy = Brain.GetEnemy())
	{
		if (UAIDirector* Director = Enemy->GetWorld()->GetSubsystem<UAIDirector>())
		{
			Director->UnregisterDesiredPosition(Enemy);
		}
	}

	OwningBrain = nullptr;
}

void UEnemyAIState_Strafe::StartQuery(UEnemyBrainComponent& Brain)
{
	Phase = EPhase::Waiting;
	WaitRemaining = RetryInterval;

	AEnemyAIController* AIController = Brain.GetAIController();
	AEnemyCharacter* Enemy = Brain.GetEnemy();
	if (!AIController || !Enemy || !AIController->StrafeQuery) return;

	// ResolveQuerierEnemyはAIController/ポーン両方解けるが、
	// 生成器のQuerier中心グリッドが敵基準になるようポーンを渡す
	FEnvQueryRequest Request(AIController->StrafeQuery, Enemy);

	++QueryGeneration;
	Request.Execute(EEnvQueryRunMode::SingleResult, this, &UEnemyAIState_Strafe::OnQueryFinished);

	Phase = EPhase::Querying;
}

void UEnemyAIState_Strafe::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	// Exit済み、または新しいクエリに置き換わっている
	if (!OwningBrain || Phase != EPhase::Querying) return;

	if (!Result.IsValid() || !Result->IsSuccessful() || Result->Items.Num() == 0)
	{
		// 移動先が見つからない: 少し待って再試行する
		// ここで接近へ抜けさせると間合いと接近を往復するので、状態は変えない
		Phase = EPhase::Waiting;
		WaitRemaining = RetryInterval;
		return;
	}

	const FVector MoveTarget = Result->GetItemAsLocation(0);

	// 味方が同じ場所へ寄らないようDirectorへ予約する
	// (BTTask_RegisterMoveTarget相当)
	if (AEnemyCharacter* Enemy = OwningBrain->GetEnemy())
	{
		if (UAIDirector* Director = Enemy->GetWorld()->GetSubsystem<UAIDirector>())
		{
			Director->RegisterDesiredPosition(Enemy, MoveTarget);
		}
	}

	// bCanStrafe: 移動方向ではなく注視 (ターゲット) に体を向けたまま横移動させる
	// これを外すと回り込み中に進行方向を向いてしまい、
	// ABPのストレイフBlendSpaceが意味を成さない
	if (OwningBrain->RequestMoveTo(MoveTarget, /*AcceptanceRadius=*/50.0f, /*bCanStrafe=*/true))
	{
		Phase = EPhase::Moving;
	}
	else
	{
		Phase = EPhase::Waiting;
		WaitRemaining = RetryInterval;
	}
}

FString UEnemyAIState_Strafe::GetDebugText() const
{
	switch (Phase)
	{
	case EPhase::Querying: return TEXT("EQS");
	case EPhase::Moving:   return TEXT("move");
	default:               return FString::Printf(TEXT("wait %.1fs"), WaitRemaining);
	}
}
