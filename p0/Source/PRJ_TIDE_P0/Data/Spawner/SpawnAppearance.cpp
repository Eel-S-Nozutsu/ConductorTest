// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "SpawnAppearance.h"
#include "PRJ_TIDE_P0/Actors/Spawner/TideSpawnPoint.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

void UCliffJumpAppearance::Execute(AEnemyCharacter* Enemy, const TArray<FTransform>& Points)
{
	if (!Enemy || !LandingPoint || Points.IsEmpty()) return;

	UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement();

	// 負値
	const float GravityZ = CMC->GetGravityZ();

	// 落下中は行動させない。解除は着地時のRestartAIと対になる
	Enemy->SetReacting(true, TEXT("CliffJump"));
	if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
	{
		AIC->StopMovement();
	}

	// Start: 現在のカプセル中心。Target: 着地点をカプセル中心高さに換算
	const FVector Start      = Enemy->GetActorLocation();
	const float HalfHeight   = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Target     = LandingPoint->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight);

	const FVector Delta    = Target - Start;
	const FVector HorizDir = FVector(Delta.X, Delta.Y, 0.0f);
	const float HorizDist  = HorizDir.Size();

	const float AbsGravity = FMath::Abs(GravityZ);

	// 着地点より必ず上になるよう下限クランプ
	const float PeakH = FMath::Max(PeakHeightOffset, Delta.Z + 1.0f);

	// 頂点高さから初速・飛行時間を逆算(距離によらず常に同じ高さ)
	const float VZ       = FMath::Sqrt(2.0f * AbsGravity * PeakH);
	const float TPeak    = VZ / AbsGravity;
	const float TDescend = FMath::Sqrt(2.0f * (PeakH - Delta.Z) / AbsGravity);
	const float FlightTime = TPeak + TDescend;

	const FVector VXY = HorizDist > 0.0f
		? HorizDir.GetSafeNormal() * (HorizDist / FlightTime)
		: FVector::ZeroVector;

	JumpStartPos  = Start;
	JumpInitVel   = FVector(VXY.X, VXY.Y, VZ);
	JumpGravityZ  = GravityZ;
	JumpDuration  = FlightTime;
	JumpStartTime = Enemy->GetWorld()->GetTimeSeconds();
	CachedEnemy   = Enemy;

	Enemy->GetWorldTimerManager().ClearTimer(JumpTickHandle);
	Enemy->GetWorldTimerManager().SetTimer(JumpTickHandle,
		FTimerDelegate::CreateUObject(this, &UCliffJumpAppearance::TickJump),
		1.0f / 60.0f, true);

	if (JumpMontage)
	{
		Enemy->PlayAnimMontage(JumpMontage);
	}
}

void UCliffJumpAppearance::TickJump()
{
	if (!CachedEnemy.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(JumpTickHandle);
		}
		return;
	}

	const float Elapsed = CachedEnemy->GetWorld()->GetTimeSeconds() - JumpStartTime;
	const float T = FMath::Min(Elapsed, JumpDuration);

	const FVector NewPos = JumpStartPos
		+ FVector(JumpInitVel.X, JumpInitVel.Y, 0.0f) * T
		+ FVector(0.0f, 0.0f, JumpInitVel.Z * T + 0.5f * JumpGravityZ * T * T);

	// カプセルスウィープで床衝突を検出
	UCapsuleComponent* Capsule = CachedEnemy->GetCapsuleComponent();
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedEnemy.Get());

	FHitResult SweepHit;
	const bool bHit = CachedEnemy->GetWorld()->SweepSingleByChannel(
		SweepHit,
		CachedEnemy->GetActorLocation(),
		NewPos,
		FQuat::Identity,
		ECC_WorldStatic,
		CapsuleShape,
		QueryParams);

	if (bHit || Elapsed >= JumpDuration)
	{
		CachedEnemy->SetActorLocation(bHit ? SweepHit.Location : NewPos, false);
		FinishJump();
		return;
	}

	CachedEnemy->SetActorLocation(NewPos, false);
}

void UCliffJumpAppearance::FinishJump()
{
	CachedEnemy->GetWorldTimerManager().ClearTimer(JumpTickHandle);
	CachedEnemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (LandMontage)
	{
		CachedEnemy->PlayAnimMontage(LandMontage);

		if (UAnimInstance* AnimInst = CachedEnemy->GetMesh()->GetAnimInstance())
		{
			AnimInst->OnMontageEnded.AddDynamic(this, &UCliffJumpAppearance::OnLandMontageEnded);
		}
		// CachedEnemyはOnLandMontageEndedまで保持する
		return;
	}

	RestartAI();
	CachedEnemy = nullptr;
}

void UCliffJumpAppearance::OnLandMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!CachedEnemy.IsValid()) return;
	if (Montage != LandMontage) return;

	if (UAnimInstance* AnimInst = CachedEnemy->GetMesh()->GetAnimInstance())
	{
		AnimInst->OnMontageEnded.RemoveDynamic(this, &UCliffJumpAppearance::OnLandMontageEnded);
	}

	RestartAI();
	CachedEnemy = nullptr;
}

void UCliffJumpAppearance::RestartAI()
{
	if (!CachedEnemy.IsValid()) return;

	CachedEnemy->SetReacting(false);
}
