// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#include "SpecialProjectileBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

void USpecialProjectileBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// プール再利用に備え実行時状態をすべて初期化
	Target = nullptr;
	Elapsed = 0.0f;
	HomingRate = 0.0f;

	if (!Projectile) return;

	// 追従対象はプレイヤーPawnを自動取得する
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(Projectile, 0))
	{
		Target = Player->GetRootComponent();
	}
}

void USpecialProjectileBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile || !Projectile->ProjectileMovement) return;

	USceneComponent* TargetComp = Target.Get();
	if (!TargetComp) return;

	Elapsed += DeltaSeconds;

	if (Elapsed < BeginHormingTime)
	{
		return;
	}
	else if (Elapsed < EndHomingTime)
	{
		if (HomingRate < 1.0f)
		{
			HomingRate += HomingRateAccel * DeltaSeconds;
			HomingRate = FMath::Min(HomingRate, 1.0f);
		}
	}
	else
	{
		if (HomingRate > 0.0f)
		{
			HomingRate -= HomingRateAccel * DeltaSeconds;
			HomingRate = FMath::Max(HomingRate, 0.0f);
		}
	}
	const FVector CurVel = Projectile->ProjectileMovement->Velocity;
	const float Speed = CurVel.Size();
	if (Speed < KINDA_SMALL_NUMBER) return;

	const FVector CurDir = CurVel / Speed;
	const FVector ToTargetDir =
		(TargetComp->GetComponentLocation() - Projectile->GetActorLocation()).GetSafeNormal();

	if (ToTargetDir.IsNearlyZero()) return;

	FVector DesiredDir = FMath::Lerp(CurDir, ToTargetDir, HomingRate).GetSafeNormal();
	if (DesiredDir.IsNearlyZero()) return;

	const float Dot = FMath::Clamp(FVector::DotProduct(CurDir, DesiredDir), -1.0f, 1.0f);
	const float AngleRad = FMath::Acos(Dot);
	const float MaxTurnRad = FMath::DegreesToRadians(MaxTurnAngleDegPerSec) * DeltaSeconds;

	if (AngleRad > MaxTurnRad && MaxTurnRad > KINDA_SMALL_NUMBER)
	{
		FVector Axis = FVector::CrossProduct(CurDir, DesiredDir);

		// 真逆に近いと外積がほぼ0になるので、適当な直交軸を作る
		if (Axis.IsNearlyZero())
		{
			Axis = FVector::CrossProduct(CurDir, FVector::UpVector);
			if (Axis.IsNearlyZero())
			{
				Axis = FVector::CrossProduct(CurDir, FVector::RightVector);
			}
		}

		Axis.Normalize();
		DesiredDir = FQuat(Axis, MaxTurnRad).RotateVector(CurDir).GetSafeNormal();
	}

	Projectile->ProjectileMovement->Velocity = DesiredDir * Speed;
}
