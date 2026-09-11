// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Instances/Character/Enemy/EnemyAnimInstance.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UEnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// AnimGraphはBeginPlayより前に動き始めるため、ここで部位数分を事前確保する
	// CharacterDataはEditAnywhereのBPデフォルト値なので初期化時点で参照可能
	if (const APawn* Pawn = TryGetPawnOwner())
	{
		if (const ATideCharacter* TideChar = Cast<ATideCharacter>(Pawn))
		{
			if (const UBossDataAsset* BossData = Cast<UBossDataAsset>(TideChar->GetCharacterData()))
			{
				InitHitShake(BossData->Parts.Num());
			}
		}
	}
}

void UEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (APawn* Pawn = TryGetPawnOwner())
	{
		GroundSpeed = Pawn->GetVelocity().Size2D();

		FVector Velocity = Pawn->GetVelocity();
		FRotator Rotation = Pawn->GetActorRotation();

		FVector RelativeVelocity = Rotation.UnrotateVector(Velocity);

		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

			const float MaxSpeed = Movement->MaxWalkSpeed;
			if (MaxSpeed > 0.0f)
			{
				ForwardSpeed = FMath::Clamp(RelativeVelocity.X / MaxSpeed, -1.0f, 1.0f);
				RightSpeed = FMath::Clamp(RelativeVelocity.Y / MaxSpeed, -1.0f, 1.0f);
			}

			bIsFalling = Movement->IsFalling();

			bHasTarget = false;
			bIsStrafing = false;
			if (const AEnemyAIController* Controller = Cast<AEnemyAIController>(Character->GetController()))
			{
				// ストレイフの成立条件(距離・攻撃者かどうか)はフロー制御側が決める。
				// ここは結果を映すだけ
				// ここで再判定すると条件がズレて足滑りする
				AActor* Target = nullptr;
				if (const UEnemyBrainComponent* Brain = Controller->BrainStateMachine)
				{
					bIsStrafing = Brain->IsStrafing();
					Target = Brain->GetTargetActor();
				}

				if (Target)
				{
					bHasTarget = IsValid(Target);
					if (bHasTarget)
					{
						LookAtWorldPosition = Target->GetActorLocation() + LookAtHeightOffset;
					}
				}
			}

			// 視線IK抑制中(火炎放射など)は追従先があっても0へフェードする
			const float TargetAlpha = (bHasTarget && !bSuppressLookAtIK) ? 1.0f : 0.0f;
			LookAtAlpha = FMath::FInterpTo(LookAtAlpha, TargetAlpha, DeltaSeconds, 5.0f);
		}
	}

	// HitShakeスプリングダンパーomega=30, zeta=0.2の不足減衰
	// 往復振動しながらDuration内に収束する
	constexpr float Stiffness = 2000.0f;
	constexpr float Damping   = 12.0f;
	for (int32 i = 0; i < ShakeStates.Num(); ++i)
	{
		FHitShakeState& State = ShakeStates[i];
		if (State.Remaining <= 0.0f && FMath::IsNearlyZero(State.Angle, 0.1f))
		{
			State.Angle    = 0.0f;
			State.Velocity = 0.0f;
			PartShakeAngles[i] = 0.0f;
			continue;
		}
		State.Velocity += (-Stiffness * State.Angle - Damping * State.Velocity) * DeltaSeconds;
		State.Angle    += State.Velocity * DeltaSeconds;
		State.Remaining = FMath::Max(State.Remaining - DeltaSeconds, 0.0f);
		PartShakeAngles[i] = State.Angle;
	}
}

void UEnemyAnimInstance::InitHitShake(int32 PartCount)
{
	ShakeStates.SetNum(PartCount);
	PartShakeAngles.SetNum(PartCount);
}

void UEnemyAnimInstance::TriggerHitShake(int32 PartIndex, float Impulse, float Duration)
{
	if (PartIndex < 0 || Impulse <= 0.0f) return;

	const int32 RequiredSize = PartIndex + 1;
	if (ShakeStates.Num() < RequiredSize)
	{
		ShakeStates.SetNum(RequiredSize);
		PartShakeAngles.SetNum(RequiredSize);
	}

	ShakeStates[PartIndex].Velocity += Impulse;
	ShakeStates[PartIndex].Remaining = Duration;
}
