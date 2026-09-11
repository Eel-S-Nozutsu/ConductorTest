// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideAttackExecution.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

void UApproachMontageAttackExecution::OnAttackBegin(AEnemyCharacter* Enemy)
{
	CachedEnemy = Enemy;
	CachedAnimInstance = Enemy->GetMesh()->GetAnimInstance();
	bReachedTarget = false;

	AAIController* AIC = Cast<AAIController>(Enemy->GetController());
	if (!AIC)
	{
		if (FinishDelegate)
		{
			FinishDelegate(false);
			return;
		}
	}

	AActor* Target = Enemy->GetTargetActor();
	if (!Target)
	{
		if (FinishDelegate)
		{
			FinishDelegate(false);
			return;
		}
	}

	FAIMoveRequest MoveReq(Target);
	MoveReq.SetAcceptanceRadius(ApproachRadius);

	// MoveToを先に呼ぶ。AlreadyAtGoal時はMoveTo内で
	// OnRequestFinishedが同期発火するため、デリゲート登録前なら
	// PlayAttackの二重呼び出しを防げる
	const FPathFollowingRequestResult MoveResult = AIC->MoveTo(MoveReq);
	ActiveMoveRequestID = MoveResult.MoveId;

	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		PlayAttack();
	}
	else if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		if (UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent())
		{
			PFC->OnRequestFinished.AddUObject(this,
				&UApproachMontageAttackExecution::OnMoveRequestFinished);
		}
	}
	else
	{
		if (FinishDelegate) FinishDelegate(false);
	}
}

void UApproachMontageAttackExecution::OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime)
{
	if (bReachedTarget || GiveUpRange <= 0.0f) return;

	const float Dist = Enemy->GetDistToTarget();
	if (Dist < 0.0f) return;

	if (Dist > GiveUpRange)
	{
		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
		{
			if (UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent())
			{
				PFC->OnRequestFinished.RemoveAll(this);
			}
			AIC->StopMovement();
		}
		if (FinishDelegate) FinishDelegate(false);
	}
}

void UApproachMontageAttackExecution::OnAttackEnd(AEnemyCharacter* Enemy)
{
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		if (UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent())
			PFC->OnRequestFinished.RemoveAll(this);
		if (!bReachedTarget)
			AIC->StopMovement();
	}

	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(
			this, &UApproachMontageAttackExecution::OnApproachMontageEnded);
	}
}

void UApproachMontageAttackExecution::OnMoveRequestFinished(
	FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID != ActiveMoveRequestID) return;

	if (CachedEnemy.IsValid())
	{
		if (AAIController* AIC = Cast<AAIController>(CachedEnemy->GetController()))
		{
			if (UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent())
			{
				PFC->OnRequestFinished.RemoveAll(this);
			}
		}
	}

	if (Result.IsSuccess())
	{
		PlayAttack();
	}
	else if (FinishDelegate)
	{
		FinishDelegate(false);
	}
}

void UApproachMontageAttackExecution::PlayAttack()
{
	bReachedTarget = true;
	if (!CachedEnemy.IsValid())
	{
		if (FinishDelegate)
		{
			FinishDelegate(false);
			return;
		}
	}

	CachedEnemy->GetCharacterMovement()->StopMovementImmediately();

	if (AttackMontage && CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->Montage_Play(AttackMontage);
		CachedAnimInstance->OnMontageEnded.AddDynamic(
			this, &UApproachMontageAttackExecution::OnApproachMontageEnded);
	}
	else if (FinishDelegate)
	{
		FinishDelegate(false);
	}
}

void UApproachMontageAttackExecution::OnApproachMontageEnded(
	UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage) return;
	if (CachedAnimInstance.IsValid())
	{
		CachedAnimInstance->OnMontageEnded.RemoveDynamic(
			this, &UApproachMontageAttackExecution::OnApproachMontageEnded);
	}
	if (FinishDelegate) FinishDelegate(!bInterrupted);
}
