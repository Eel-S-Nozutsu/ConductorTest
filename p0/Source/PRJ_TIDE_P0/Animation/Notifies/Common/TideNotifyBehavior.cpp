// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideNotifyBehavior.h"

#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"

// ------------------------------------------------------------
// Rotate To Target
// ------------------------------------------------------------

void UEnemyNotifyBehavior_RotateToTarget::OnBegin(ATideCharacter* Character, float TotalDuration)
{
	FRotateState& State = States.FindOrAdd(Character);
	State.StartYaw    = Character->GetActorRotation().Yaw;
	State.ElapsedTime = 0.0f;
	State.Duration    = TotalDuration;
}

void UEnemyNotifyBehavior_RotateToTarget::OnTick(ATideCharacter* Character, float DeltaTime)
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Character);
	if (!Enemy) return;

	// 攻撃ロジックが向きを握っている間は触らない。協調攻撃の背中合わせのように
	// 「PCを向いてはいけない」配置が、共用モンタージュのこの補正で崩れるのを防ぐ
	if (Enemy->IsActionFacingLocked()) return;

	FRotateState* State = States.Find(Character);
	if (!State) return;

	State->ElapsedTime = FMath::Min(State->ElapsedTime + DeltaTime, State->Duration);

	AAIController* AIC = Cast<AAIController>(Enemy->GetController());
	const AActor* Target = Enemy->GetTargetActor();
	if (!Target || !AIC) return;

	if (MaxRotationAngle > 0.0f)
	{
		const float Rotated = FMath::Abs(FMath::FindDeltaAngleDegrees(State->StartYaw, Enemy->GetActorRotation().Yaw));
		if (Rotated >= MaxRotationAngle) return;
	}

	const float NormalizedTime = State->Duration > 0.0f
		? FMath::Clamp(State->ElapsedTime / State->Duration, 0.0f, 1.0f)
		: 1.0f;
	const float EasedAlpha    = UKismetMathLibrary::Ease(0.0f, 1.0f, NormalizedTime, EasingFunction.GetValue(), BlendExp);
	const float EffectiveRate = RotationRate * EasedAlpha;

	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero()) return;

	const float TargetYaw = FRotationMatrix::MakeFromX(ToTarget).Rotator().Yaw + YawOffset;
	const float NewYaw    = FMath::FixedTurn(Enemy->GetActorRotation().Yaw, TargetYaw, EffectiveRate * DeltaTime);

	const FVector FocalDir = FRotator(0.0f, NewYaw, 0.0f).Vector();

	// 非ルートモーション時:SetFocalPoint経由で
	// UpdateControlRotation→PhysicsRotationに乗せる
	// (SetControlRotation単体だと別フラグで上書きされるため)
	AIC->SetFocalPoint(Enemy->GetActorLocation() + FocalDir * 1000.0f, EAIFocusPriority::Gameplay);

	// ルートモーション時:PhysicsRotationがスキップされるため
	// SetActorRotationで直接上書きする
	Enemy->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

void UEnemyNotifyBehavior_RotateToTarget::OnEnd(ATideCharacter* Character)
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Character))
	{
		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
			AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
	States.Remove(Character);
}
