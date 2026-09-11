// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "GuidanceBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UGuidanceBehavior::SetTarget(AActor* InTarget)
{
	CfgTarget = InTarget;
}

void UGuidanceBehavior::SetTargetLocation(const FVector& WorldLocation)
{
	CfgTargetPoint = WorldLocation;
	bCfgTargetPoint = true;
}

void UGuidanceBehavior::SetInitialHeadingDirection(const FVector& WorldDirection)
{
	CfgHeadingDir = WorldDirection.GetSafeNormal();
	bCfgHeading = !CfgHeadingDir.IsNearlyZero();
}

void UGuidanceBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// 実行時状態を初期化 ※プール再利用で前回値を残さない
	Elapsed = 0.0f;
	bDetonated = false;
	CurrentSpeed = FMath::Max(0.0f, Speed);

	if (!Projectile) return;

	// 誘導は自前で制御するためPMCを止める
	if (Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->StopMovementImmediately();
		Projectile->ProjectileMovement->SetComponentTickEnabled(false);
	}

	// 設定を取り込む
	TargetActor = CfgTarget;
	TargetPoint = CfgTargetPoint;
	bHasTargetPoint = bCfgTargetPoint;

	bInitialHeadingSet = bCfgHeading;
	InitialHeadingDirection = bCfgHeading ? CfgHeadingDir : Projectile->GetActorForwardVector().GetSafeNormal();

	Dir = InitialHeadingDirection.IsNearlyZero() ? Projectile->GetActorForwardVector().GetSafeNormal() : InitialHeadingDirection;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}
	Projectile->SetActorRotation(Dir.Rotation());

	// 設定フラグをリセット
	CfgTarget = nullptr;
	bCfgTargetPoint = false;
	bCfgHeading = false;
}

FVector UGuidanceBehavior::ResolveAimCenter() const
{
	const AActor* Target = TargetActor.Get();
	if (!Target)
	{
		return bHasTargetPoint ? TargetPoint : FVector::ZeroVector;
	}

	FVector Center = Target->GetActorLocation();
	if (const ACharacter* C = Cast<ACharacter>(Target))
	{
		if (C->GetCapsuleComponent())
		{
			// 足元 (中心-半身) から係数ぶん上を狙う
			const float HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Center.Z += HalfHeight * (2.0f * FMath::Clamp(TargetCenterHeightFraction, 0.0f, 1.0f) - 1.0f);
		}
	}
	return Center;
}

void UGuidanceBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (bDetonated || !Projectile) return;

	Elapsed += DeltaSeconds;

	const FVector Pos = Projectile->GetActorLocation();

	const AActor* Target = TargetActor.Get();

	// 近接信管: 目標中心へ十分近づいたら炸裂
	if (ProximityFuseRadius > KINDA_SMALL_NUMBER && Target)
	{
		if (FVector::Dist(Pos, ResolveAimCenter()) <= ProximityFuseRadius)
		{
			Detonate(Projectile, Pos, nullptr);
			Projectile->Despawn();
			return;
		}
	}

	// 狙い点を決める (目標中心、または固定点)
	FVector AimPoint;
	if (Target)
	{
		AimPoint = ResolveAimCenter();
	}
	else if (bHasTargetPoint)
	{
		AimPoint = TargetPoint;
	}
	else
	{
		AimPoint = Pos + Dir * 1000.0f; // 目標が無ければ直進
	}

	// 初期方位保持中は追尾せず直進、経過後に追尾方位へ
	FVector DesiredDir = (bInitialHeadingSet && Elapsed < FMath::Max(0.0f, HeadingHoldTime))
		? InitialHeadingDirection
		: (AimPoint - Pos).GetSafeNormal();
	if (DesiredDir.IsNearlyZero())
	{
		DesiredDir = Dir;
	}

	// 旋回レート制限でDirをDesiredDirへ寄せる
	const float MaxRad = FMath::DegreesToRadians(FMath::Max(0.0f, TurnRateDegPerSec)) * DeltaSeconds;
	const float Dot = FMath::Clamp(FVector::DotProduct(Dir, DesiredDir), -1.0f, 1.0f);
	const float Angle = FMath::Acos(Dot);
	if (MaxRad > KINDA_SMALL_NUMBER && Angle > MaxRad)
	{
		FVector Axis = FVector::CrossProduct(Dir, DesiredDir);
		if (Axis.Normalize())
		{
			Dir = FQuat(Axis, MaxRad).RotateVector(Dir).GetSafeNormal();
		}
	}
	else
	{
		Dir = DesiredDir;
	}

	// 速度更新
	CurrentSpeed += Acceleration * DeltaSeconds;
	if (MaxSpeed > KINDA_SMALL_NUMBER)
	{
		CurrentSpeed = FMath::Min(CurrentSpeed, MaxSpeed);
	}
	CurrentSpeed = FMath::Max(CurrentSpeed, 1.0f);

	const FVector NewPos = Pos + Dir * CurrentSpeed * DeltaSeconds;
	if (!MoveWithSweep(Projectile, NewPos))
	{
		// 接触はOnHit側で炸裂処理されるためここでは止めるだけ
		return;
	}

	Projectile->SetActorRotation(Dir.Rotation());
}

void UGuidanceBehavior::OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result)
{
	Detonate(Projectile, Hit.ImpactPoint, HitActor);
}

void UGuidanceBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	if (!Projectile || Projectile->WasHit() || Projectile->GetEndReason() != EProjectileEndReason::LifeSpanExpired)
	{
		return;
	}
	Detonate(Projectile, Projectile->GetActorLocation(), nullptr);
}

bool UGuidanceBehavior::MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation)
{
	if (!Projectile)
	{
		return false;
	}

	FHitResult SweepHit;
	Projectile->SetActorLocation(NewLocation, true, &SweepHit);
	return !SweepHit.bBlockingHit;
}

void UGuidanceBehavior::Detonate(AEnemyProjectile* Projectile, const FVector& Center, AActor* DirectHitActor)
{
	if (bDetonated || !Projectile) return;
	bDetonated = true;

	if (SplashRadius <= KINDA_SMALL_NUMBER || SplashDamage <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = Projectile->GetOwner();

	TArray<AActor*> IgnoreActors = { OwnerActor };
	if (DirectHitActor)
	{
		IgnoreActors.Add(DirectHitActor);
	}

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(Projectile, Center, SplashRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn),
		  UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) },
		nullptr, IgnoreActors, OverlapActors);

	for (AActor* Actor : OverlapActors)
	{
		if (!TideCombatUtil::IsHostileTo(OwnerActor, Actor)) continue;

		IDamageable* Damageable = Cast<IDamageable>(Actor);
		if (!Damageable) continue;

		FDamageInfo Info;
		Info.BaseDamage = SplashDamage;
		Info.Instigator = OwnerActor;
		Info.HitReactionTag = Projectile->HitReactionTag;
		Info.HitResult.ImpactPoint = Actor->GetActorLocation();
		Info.HitResult.ImpactNormal = (Actor->GetActorLocation() - Center).GetSafeNormal();
		Damageable->ReceiveDamage(Info);
	}
}
