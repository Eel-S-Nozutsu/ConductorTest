// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_BossIdle.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/BossCharacter.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Enemy/EnemyAnimInstance.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"

#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"

void UEnemyAIState_BossIdle::Enter(UEnemyBrainComponent& Brain)
{
	Super::Enter(Brain);

	// 基底は注視を張るが、ボスの向きは回頭モンタージュ (TickTurn) が握る
	// 注視を残すとコントローラ回転が滑らかに追従してしまい、回頭モーションが再生されなくなる
	if (AEnemyAIController* AIController = Brain.GetAIController())
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	ResetStepInterval(Brain.GetEnemyData());
}

void UEnemyAIState_BossIdle::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	Super::Tick(Brain, DeltaSeconds);

	TickTurn(Brain, DeltaSeconds);
	TickStep(Brain, DeltaSeconds);
}

void UEnemyAIState_BossIdle::TickTurn(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	ABossCharacter* Boss = Cast<ABossCharacter>(Brain.GetEnemy());
	const UBossDataAsset* Data = Boss ? Cast<UBossDataAsset>(Boss->GetCharacterData()) : nullptr;
	if (!Data) return;

	AActor* Target = Brain.GetTargetActor();
	if (!Target) return;

	const FVector Forward = Boss->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();

	const float Dot = FVector::DotProduct(Forward, ToTarget);
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

	const FBossTurnAnimSettings& TurnAnim = Data->TurnAnim;

	// 閾値を超えている間だけクールダウンを減らす (すぐ正面に戻る揺れでは回頭しない)
	if (Angle >= TurnAnim.Threshold90)
	{
		TurnCooldownRemaining -= DeltaSeconds;
	}

	if (TurnCooldownRemaining > 0.0f) return;
	if (Angle < TurnAnim.Threshold90) return;

	UEnemyAnimInstance* AnimInst = Cast<UEnemyAnimInstance>(Boss->GetMesh()->GetAnimInstance());
	if (!AnimInst) return;
	if (AnimInst->bIsFalling) return;
	if (AnimInst->IsAnyMontagePlaying()) return;

	// 左右判定: 外積Z成分が正なら右、負なら左
	const bool bTurnRight = FVector::CrossProduct(Forward, ToTarget).Z > 0.0f;

	UAnimMontage* Selected = Angle >= TurnAnim.Threshold180
		? (bTurnRight ? TurnAnim.Turn180Right : TurnAnim.Turn180Left)
		: (bTurnRight ? TurnAnim.Turn90Right  : TurnAnim.Turn90Left);
	if (!Selected) return;

	AnimInst->Montage_Play(Selected);
	TurnCooldownRemaining = TurnAnim.Cooldown;

	// ターゲット方向のYawを目標にC++側で回転を駆動する
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	Boss->BeginTurn(Selected, TargetYaw, TurnAnim.RotationRate);
}

void UEnemyAIState_BossIdle::TickStep(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	ABossCharacter* Boss = Cast<ABossCharacter>(Brain.GetEnemy());
	const UEnemyDataAsset* Data = Brain.GetEnemyData();
	if (!Boss || !Data) return;

	AActor* Target = Brain.GetTargetActor();

	switch (Data->StepAnim.BossStepTrigger)
	{
	case EBossStepTriggerPattern::Pressure:
		// 被弾が閾値まで溜まったら後退して間合いを作り直す
		if (Boss->GetStepPressure() < Data->StepAnim.StepPressureThreshold) return;
		if (TryPlayStep(Boss, Data, Target))
		{
			Boss->ResetStepPressure();
		}
		break;

	case EBossStepTriggerPattern::Interval:
		TimeUntilNextStep -= DeltaSeconds;
		if (TimeUntilNextStep > 0.0f) return;

		ResetStepInterval(Data);
		TryPlayStep(Boss, Data, Target);
		break;

	// Proximity/PostAttackは未実装 (BT時代から)
	default:
		break;
	}
}

bool UEnemyAIState_BossIdle::TryPlayStep(ABossCharacter* Boss, const UEnemyDataAsset* Data,
	AActor* Target) const
{
	UEnemyAnimInstance* AnimInst = Cast<UEnemyAnimInstance>(Boss->GetMesh()->GetAnimInstance());
	if (!AnimInst) return false;
	if (AnimInst->bIsFalling) return false;
	if (AnimInst->IsAnyMontagePlaying()) return false;

	UAnimMontage* Selected = nullptr;
	FVector StepDir = FVector::ZeroVector;

	if (Target)
	{
		// プレイヤーから遠ざかる方向をボスのローカル軸へ射影し、支配軸のモンタージュを選ぶ
		const FVector AwayFromPlayer =
			-(Target->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		const float DotFwd = FVector::DotProduct(Boss->GetActorForwardVector(), AwayFromPlayer);
		const float DotRgt = FVector::DotProduct(Boss->GetActorRightVector(), AwayFromPlayer);

		if (FMath::Abs(DotFwd) >= FMath::Abs(DotRgt))
		{
			if (DotFwd >= 0.0f) { Selected = Data->StepAnim.Forward;  StepDir =  Boss->GetActorForwardVector(); }
			else                { Selected = Data->StepAnim.Backward; StepDir = -Boss->GetActorForwardVector(); }
		}
		else
		{
			if (DotRgt >= 0.0f) { Selected = Data->StepAnim.Right; StepDir =  Boss->GetActorRightVector(); }
			else                { Selected = Data->StepAnim.Left;  StepDir = -Boss->GetActorRightVector(); }
		}
	}
	else
	{
		Selected = Data->StepAnim.Backward;
		StepDir  = -Boss->GetActorForwardVector();
	}

	if (!Selected) return false;

	// ステップ先にNavMeshがあるか確認する (崖へ下がらない)
	if (Data->StepAnim.NavCheckDistance > 0.0f)
	{
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Boss->GetWorld()))
		{
			const FVector FootPos = Boss->GetActorLocation()
				- FVector(0.0f, 0.0f, Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector CheckPos = FootPos + StepDir * Data->StepAnim.NavCheckDistance;
			FNavLocation NavLoc;
			const bool bOnNav = NavSys->ProjectPointToNavigation(
				CheckPos, NavLoc, FVector(50.0f, 50.0f, 200.0f));

#if !UE_BUILD_SHIPPING
			if (UTideGameSettings::Get()->bDebugEnemyDodgeNavCheck)
			{
				DrawDebugSphere(Boss->GetWorld(), CheckPos + FVector(0.0f, 0.0f, 30.0f), 20.0f, 8,
					bOnNav ? FColor::Green : FColor::Red, false, 1.0f);
				if (bOnNav)
				{
					DrawDebugSphere(Boss->GetWorld(), NavLoc.Location + FVector(0.0f, 0.0f, 30.0f), 15.0f, 8,
						FColor::Yellow, false, 1.0f);
				}
			}
#endif

			if (!bOnNav) return false;
		}
	}

	AnimInst->Montage_Play(Selected);
	return true;
}

void UEnemyAIState_BossIdle::ResetStepInterval(const UEnemyDataAsset* Data)
{
	TimeUntilNextStep = Data
		? FMath::RandRange(Data->StepAnim.MinInterval, Data->StepAnim.MaxInterval)
		: 3.0f;
}
