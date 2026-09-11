// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "DeployHomingBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

void UDeployHomingBehavior::SetDeployTarget(const FVector& WorldLocation)
{
	DeployTargetLocation = WorldLocation;
	bCustomDeployTarget = true;
}

void UDeployHomingBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	Phase = EHomingPhase::Deploying;
	HoverElapsed = 0.0f;
	AimElapsed = 0.0f;
	HomingElapsed = 0.0f;
	bHoming = false;

	if (!Projectile || !Projectile->ProjectileMovement) return;

	// 展開中はProjectileMovementを止め自前で位置を動かす
	Projectile->ProjectileMovement->StopMovementImmediately();
	Projectile->ProjectileMovement->SetComponentTickEnabled(false);

	SpawnLocation = Projectile->GetActorLocation();
	CachedPlayer = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(Projectile, 0));

	if (!bCustomDeployTarget)
	{
		DeployTargetLocation = Projectile->GetActorLocation() + Projectile->GetActorForwardVector() * DeployDistance;
	}
	bCustomDeployTarget = false;

	// 初速はボスの後方向き ※背中から射出する演出
	DeployVelocity = -Projectile->GetActorForwardVector() * DeploySpeed;
}

void UDeployHomingBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile) return;

	switch (Phase)
	{
	case EHomingPhase::Deploying:
	{
		const FVector ToTarget = DeployTargetLocation - Projectile->GetActorLocation();
		const float   DistToTarget = ToTarget.Size();

		if (DistToTarget <= DeployAcceptRadius)
		{
			Projectile->SetActorLocation(DeployTargetLocation);
			HoverBaseLocation = DeployTargetLocation;
			HoverElapsed = 0.0f;
			Phase = EHomingPhase::Hovering;
			break;
		}

		// 速度方向を目標へ向けて旋回
		const FVector TargetDir = ToTarget.GetSafeNormal();
		const FVector CurrentDir = DeployVelocity.GetSafeNormal();
		const FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, TargetDir, DeltaSeconds, DeployTurnRate);
		DeployVelocity = NewDir * DeploySpeed;

		// 機体の向きを速度方向に追従
		Projectile->SetActorRotation(DeployVelocity.Rotation());
		Projectile->SetActorLocation(Projectile->GetActorLocation() + DeployVelocity * DeltaSeconds);
		break;
	}
	case EHomingPhase::Hovering:
	{
		HoverElapsed += DeltaSeconds;
		const float BobZ = HoverBobAmplitude * FMath::Sin(HoverElapsed * HoverBobFrequency * TWO_PI);
		Projectile->SetActorLocation(HoverBaseLocation + FVector(0.0f, 0.0f, BobZ));

		if (HoverElapsed >= HoverDuration)
		{
			Phase = EHomingPhase::Aiming;
			AimElapsed = 0.0f;
		}
		break;
	}
	case EHomingPhase::Aiming:
	{
		AimElapsed += DeltaSeconds;

		ACharacter* Player = CachedPlayer.Get();
		if (!Player) { Launch(Projectile); break; }

		const FVector ToPlayer = (Player->GetActorLocation() - Projectile->GetActorLocation()).GetSafeNormal();
		Projectile->SetActorRotation(FMath::RInterpConstantTo(
			Projectile->GetActorRotation(), ToPlayer.Rotation(), DeltaSeconds, AimRotationRate));

		if (AimElapsed >= AimDuration) Launch(Projectile);
		break;
	}
	case EHomingPhase::Flying:
	{
		if (!bHoming) break;

		HomingElapsed += DeltaSeconds;

		const bool bTimeUp = (HomingDuration > 0.0f) && (HomingElapsed >= HomingDuration);
		const bool bDistanceOver = (HomingMaxDistance > 0.0f) && (FVector::Dist(SpawnLocation, Projectile->GetActorLocation()) >= HomingMaxDistance);

		if (bTimeUp || bDistanceOver)
		{
			StopHoming(Projectile);
		}
		break;
	}
	default: break;
	}
}

void UDeployHomingBehavior::Launch(AEnemyProjectile* Projectile)
{
	Phase = EHomingPhase::Flying;

	if (!Projectile || !Projectile->ProjectileMovement) return;
	UProjectileMovementComponent* PMC = Projectile->ProjectileMovement;

	PMC->SetComponentTickEnabled(true);
	PMC->MaxSpeed = Projectile->InitialSpeed;
	PMC->Velocity = Projectile->GetActorForwardVector() * Projectile->InitialSpeed;

	if (ACharacter* Player = CachedPlayer.Get())
	{
		PMC->bIsHomingProjectile = true;
		PMC->HomingAccelerationMagnitude = HomingAcceleration;
		PMC->HomingTargetComponent = Player->GetRootComponent();
	}

	bHoming = true;
}

void UDeployHomingBehavior::StopHoming(AEnemyProjectile* Projectile)
{
	bHoming = false;
	if (Projectile && Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->bIsHomingProjectile = false;
	}
}
