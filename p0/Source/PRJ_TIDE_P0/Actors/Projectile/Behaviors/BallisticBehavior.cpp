// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "BallisticBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "GameFramework/ProjectileMovementComponent.h"

void UBallisticBehavior::SetTargetLocation(const FVector& WorldLocation)
{
	CfgTarget = WorldLocation;
	bCfgTarget = true;
}

void UBallisticBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	if (!Projectile) return;

	const FVector Start = Projectile->GetActorLocation();
	Target = bCfgTarget ? CfgTarget : (Start + Projectile->GetActorForwardVector() * 1000.0f);

	UWorld* World = Projectile->GetWorld();
	const float WorldGravityZ = World ? World->GetGravityZ() : -980.0f;
	const float GravityZ = WorldGravityZ * FMath::Max(0.0f, GravityScale);

	// 着弾点へFlightDuration秒で到達する初速を逆算する:
	//   Target = Start + v0 * T + 0.5 * g * T^2  →  v0 = (Target - Start)/T - 0.5 * g * T
	const float T = FMath::Max(FlightDuration, 0.05f);
	const FVector GravityVec(0.0f, 0.0f, GravityZ);
	const FVector LaunchVelocity = (Target - Start) / T - 0.5f * GravityVec * T;

	if (Projectile->ProjectileMovement)
	{
		UProjectileMovementComponent* PMC = Projectile->ProjectileMovement;
		PMC->ProjectileGravityScale = FMath::Max(0.0f, GravityScale);
		// MaxSpeedは初速逆算値でクランプされないよう解除 ※0で無制限
		PMC->MaxSpeed = 0.0f;
		// 姿勢は速度方向へ追従させ、自前の毎フレーム回転を不要にする
		PMC->bRotationFollowsVelocity = true;
		PMC->SetComponentTickEnabled(true);
		PMC->Activate();
		PMC->Velocity = LaunchVelocity;
		PMC->UpdateComponentVelocity();
	}

	if (!LaunchVelocity.IsNearlyZero())
	{
		Projectile->SetActorRotation(LaunchVelocity.Rotation());
	}

	bCfgTarget = false;
}
