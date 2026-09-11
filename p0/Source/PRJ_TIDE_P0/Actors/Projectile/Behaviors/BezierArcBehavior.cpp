// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "BezierArcBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UBezierArcBehavior::SetTargetLocation(const FVector& WorldLocation)
{
	CfgP3 = WorldLocation;
	bCfgTarget = true;
}

void UBezierArcBehavior::SetControlPoint(const FVector& WorldLocation)
{
	CfgP1 = WorldLocation;
	bCfgControl = true;
	bCfgDual = false;
}

void UBezierArcBehavior::SetControlPoints(const FVector& InP1, const FVector& InP2)
{
	CfgP1 = InP1;
	CfgP2 = InP2;
	bCfgControl = true;
	bCfgDual = true;
}

void UBezierArcBehavior::SetInitialHeadingDirection(const FVector& WorldDirection)
{
	CfgHeadingDir = WorldDirection.GetSafeNormal();
	bCfgHeading = !CfgHeadingDir.IsNearlyZero();
}

void UBezierArcBehavior::SetDeploy(const FVector& InDeployLocation, float InHoverDuration)
{
	CfgDeployLoc = InDeployLocation;
	CfgHoverDur  = FMath::Max(0.0f, InHoverDuration);
	bCfgDeploy   = true;
}

void UBezierArcBehavior::SetHomingHandoff(AActor* InTarget, float InHandoffRatio, float InTurnRateDegPerSec)
{
	CfgHomingTarget = InTarget;
	CfgHandoffRatio = FMath::Clamp(InHandoffRatio, 0.0f, 1.0f);
	CfgHandoffTurn  = FMath::Max(0.0f, InTurnRateDegPerSec);
}

void UBezierArcBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// 実行時状態を初期化
	Elapsed = 0.0f;
	HoverElapsed = 0.0f;
	AimElapsed = 0.0f;
	HomingElapsed = 0.0f;
	FiringSpeedSum = 0.0f;
	FiringSpeedSampleCount = 0;
	bFrameInitialized = false;
	FrameTangent = FVector::ForwardVector;
	FrameNormal = FVector::RightVector;
	FrameBinormal = FVector::UpVector;
	HomingDir = FVector::ForwardVector;
	HomingMoveSpeed = 0.0f;
	NoiseSeedU = FMath::FRandRange(-1000.0f, 1000.0f);
	NoiseSeedV = FMath::FRandRange(-1000.0f, 1000.0f);

	if (!Projectile) return;

	// ベジェ軌道を自前で制御するため誘導中だけPMCを止める
	if (Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->StopMovementImmediately();
		Projectile->ProjectileMovement->SetComponentTickEnabled(false);
	}

	P0 = Projectile->GetActorLocation();

	// 設定を実行時状態へ取り込む
	P3 = bCfgTarget ? CfgP3 : (P0 + Projectile->GetActorForwardVector() * 1000.0f);

	bInitialHeadingSet = bCfgHeading;
	InitialHeadingDirection = CfgHeadingDir;

	HomingTarget = CfgHomingTarget;
	HomingHandoffRatio = CfgHandoffRatio;
	HomingHandoffTurnRateDegPerSec = CfgHandoffTurn;

	bUseDeploy = bCfgDeploy;
	DeployLocation = CfgDeployLoc;
	DeployHoverDuration = CfgHoverDur;

	bCustomControlPoint = bCfgControl;
	bDualControlPoints = bCfgDual;

	if (bUseDeploy)
	{
		// まずDeployLocationへ飛び待機してから本発射に移行
		// 制御点は発射開始時(StartFiringFrom)に使用 指定済みならそれを保持
		Phase = EArcPhase::Deploying;
		if (bCustomControlPoint)
		{
			P1 = CfgP1;
			P2 = bDualControlPoints ? CfgP2 : CfgP1;
		}
		const FVector ToDeploy = (DeployLocation - P0).GetSafeNormal();
		if (!ToDeploy.IsNearlyZero())
		{
			Projectile->SetActorRotation(ToDeploy.Rotation());
		}
	}
	else
	{
		// 即発射 (ベジェ)
		Phase = EArcPhase::Firing;
		if (!bCustomControlPoint)
		{
			const FVector Delta = P3 - P0;
			const FVector Up(0.0f, 0.0f, 1.0f);
			P1 = P0 + Delta * (1.0f / 3.0f) + Up * ArcHeight;
			P2 = P0 + Delta * (2.0f / 3.0f) + Up * (ArcHeight * 0.35f);
		}
		else if (!bDualControlPoints)
		{
			const FVector Guide = CfgP1;
			P1 = FMath::Lerp(P0, Guide, 0.75f);
			P2 = FMath::Lerp(P3, Guide, 0.75f);
		}
		else
		{
			P1 = CfgP1;
			P2 = CfgP2;
		}
	}

	// 設定フラグをリセット ※プール再利用で前回値を残さない
	bCfgTarget = false;
	bCfgControl = false;
	bCfgDual = false;
	bCfgHeading = false;
	bCfgDeploy = false;
	CfgHandoffRatio = 0.0f;
	CfgHomingTarget = nullptr;
}

void UBezierArcBehavior::StartFiringFrom(const FVector& Origin)
{
	P0 = Origin;
	if (!bCustomControlPoint)
	{
		const FVector Delta = P3 - P0;
		const FVector Up(0.0f, 0.0f, 1.0f);
		P1 = P0 + Delta * (1.0f / 3.0f) + Up * ArcHeight;
		P2 = P0 + Delta * (2.0f / 3.0f) + Up * (ArcHeight * 0.35f);
	}
	Elapsed = 0.0f;
	FiringSpeedSum = 0.0f;
	FiringSpeedSampleCount = 0;
	bFrameInitialized = false;
	Phase = EArcPhase::Firing;
}

void UBezierArcBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile) return;

	switch (Phase)
	{
	case EArcPhase::Deploying: TickDeploy(Projectile, DeltaSeconds); return;
	case EArcPhase::Hovering: TickHover(Projectile, DeltaSeconds); return;
	case EArcPhase::Aiming: TickAim(Projectile, DeltaSeconds); return;
	case EArcPhase::Homing: TickHoming(Projectile, DeltaSeconds); return;
	case EArcPhase::Inertial: return;
	default: break;
	}

	TickFiring(Projectile, DeltaSeconds);
}

void UBezierArcBehavior::TickDeploy(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector Pos = Projectile->GetActorLocation();
	const FVector ToDeploy = DeployLocation - Pos;
	const float Dist = ToDeploy.Size();
	if (Dist <= FMath::Max(1.0f, DeployAcceptRadius))
	{
		Projectile->SetActorLocation(DeployLocation);
		HoverBaseLocation = DeployLocation;
		HoverElapsed = 0.0f;
		Phase = EArcPhase::Hovering;
		return;
	}

	const FVector Dir = ToDeploy / Dist;
	const float Step = FMath::Min(Dist, FMath::Max(0.0f, DeploySpeed) * DeltaSeconds);
	if (!MoveWithSweep(Projectile, Pos + Dir * Step))
	{
		return;
	}
	Projectile->SetActorRotation(Dir.Rotation());
}

void UBezierArcBehavior::TickHover(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	HoverElapsed += DeltaSeconds;

	const float Bob = FMath::Sin(HoverElapsed * 2.0f * PI * 1.5f) * FMath::Max(0.0f, DeployHoverBobAmplitude);
	if (!MoveWithSweep(Projectile, HoverBaseLocation + FVector(0.0f, 0.0f, Bob)))
	{
		return;
	}

	if (HoverElapsed >= DeployHoverDuration)
	{
		if (AimDuration > 0.0f)
		{
			AimElapsed = 0.0f;
			Phase = EArcPhase::Aiming;
		}
		else
		{
			StartFiringFrom(Projectile->GetActorLocation());
		}
	}
}

void UBezierArcBehavior::TickAim(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	AimElapsed += DeltaSeconds;

	const FVector ToAim = (P1 - Projectile->GetActorLocation()).GetSafeNormal();
	if (!ToAim.IsNearlyZero())
	{
		Projectile->SetActorRotation(FMath::RInterpConstantTo(
			Projectile->GetActorRotation(), ToAim.Rotation(), DeltaSeconds, FMath::Max(0.0f, AimRotationRate)));
	}

	if (AimElapsed >= FMath::Max(0.0f, AimDuration))
	{
		StartFiringFrom(Projectile->GetActorLocation());
	}
}

FVector UBezierArcBehavior::HomingTargetCenter(AEnemyProjectile* Projectile) const
{
	const AActor* Target = HomingTarget.Get();
	if (!Target) return Projectile->GetActorLocation();
	FVector Center = Target->GetActorLocation();
	if (const ACharacter* C = Cast<ACharacter>(Target))
	{
		if (C->GetCapsuleComponent())
		{
			Center.Z += C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f;
		}
	}
	return Center;
}

void UBezierArcBehavior::TickHoming(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector Pos = Projectile->GetActorLocation();

	HomingElapsed += DeltaSeconds;

	if (HomingElapsed >= FMath::Max(0.5f, HomingMaxDuration) || !HomingTarget.IsValid())
	{
		EnterInertialFlight(Projectile, HomingDir * FMath::Max(1.0f, HomingMoveSpeed));
		return;
	}

	const FVector ToTarget = HomingTargetCenter(Projectile) - Pos;
	const float Dist = ToTarget.Size();
	const FVector DesiredDir = ToTarget / FMath::Max(Dist, KINDA_SMALL_NUMBER);
	const float MaxRad = FMath::DegreesToRadians(HomingHandoffTurnRateDegPerSec) * DeltaSeconds;
	const float Dot = FMath::Clamp(FVector::DotProduct(HomingDir, DesiredDir), -1.0f, 1.0f);
	const float Angle = FMath::Acos(Dot);
	if (Angle > MaxRad)
	{
		FVector Axis = FVector::CrossProduct(HomingDir, DesiredDir);
		if (!Axis.Normalize())
		{
			Axis = FVector::UpVector;
		}
		HomingDir = FQuat(Axis, MaxRad).RotateVector(HomingDir).GetSafeNormal();
	}
	else
	{
		HomingDir = DesiredDir;
	}

	const float Speed = FMath::Max(1.0f, HomingMoveSpeed);
	const FVector NewPos = Pos + HomingDir * Speed * DeltaSeconds;

	if (!MoveWithSweep(Projectile, NewPos))
	{
		return;
	}

	Projectile->SetActorRotation(HomingDir.Rotation());
}

void UBezierArcBehavior::EnterInertialFlight(AEnemyProjectile* Projectile, const FVector& InitialVelocity)
{
	if (!Projectile)
	{
		return;
	}

	if (Projectile->ProjectileMovement)
	{
		const float Speed = InitialVelocity.Size();
		if (Speed > KINDA_SMALL_NUMBER)
		{
			Projectile->ProjectileMovement->InitialSpeed = Speed;
			Projectile->ProjectileMovement->MaxSpeed = Speed;
		}
		Projectile->ProjectileMovement->SetComponentTickEnabled(true);
		Projectile->ProjectileMovement->Activate();
		Projectile->ProjectileMovement->Velocity = InitialVelocity;
		Projectile->ProjectileMovement->UpdateComponentVelocity();
	}

	Phase = EArcPhase::Inertial;
}

bool UBezierArcBehavior::MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation)
{
	if (!Projectile)
	{
		return false;
	}

	FHitResult SweepHit;
	Projectile->SetActorLocation(NewLocation, true, &SweepHit);
	return !SweepHit.bBlockingHit;
}

void UBezierArcBehavior::TickFiring(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector PrevPos = Projectile->GetActorLocation();

	Elapsed += DeltaSeconds;
	const float t = FMath::Clamp(Elapsed / FMath::Max(FlightDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const FVector Tangent = EvaluateCubicTangent(t).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		if (!bFrameInitialized) InitializeFrame(Tangent);
		else                    UpdateFrame(Tangent);
	}

	const FVector BasePos = EvaluateCubicPosition(t);
	FVector NewPos = BasePos + EvaluateWeaveOffset(t);
	if (t >= 1.0f)
	{
		NewPos = P3;
	}

	if (!MoveWithSweep(Projectile, NewPos))
	{
		return;
	}

	// 巡航速度(平均)算出用にこのステップの速度を蓄積する
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		FiringSpeedSum += (NewPos - PrevPos).Size() / DeltaSeconds;
		++FiringSpeedSampleCount;
	}

	// 姿勢は実移動方向へ追従 射出直後は初期方位を保持
	FVector MoveDir = (NewPos - PrevPos).GetSafeNormal();
	if (MoveDir.IsNearlyZero())
	{
		MoveDir = FrameTangent;
	}

	const bool bUseHoldHeading = bInitialHeadingSet && (Elapsed < FMath::Max(HeadingHoldTime, 0.0f));
	const FVector DesiredDir = bUseHoldHeading ? InitialHeadingDirection : MoveDir;

	FVector CurrentForward = Projectile->GetActorForwardVector().GetSafeNormal();
	if (CurrentForward.IsNearlyZero())
	{
		CurrentForward = DesiredDir;
	}

	const float MaxTurnRadians = FMath::DegreesToRadians(FMath::Max(TurnRateDegPerSec, 0.0f)) * DeltaSeconds;
	FVector FinalForward = DesiredDir;
	if (MaxTurnRadians > KINDA_SMALL_NUMBER)
	{
		const float Dot = FMath::Clamp(FVector::DotProduct(CurrentForward, DesiredDir), -1.0f, 1.0f);
		const float Angle = FMath::Acos(Dot);
		if (Angle > MaxTurnRadians)
		{
			FVector Axis = FVector::CrossProduct(CurrentForward, DesiredDir);
			if (Axis.Normalize())
			{
				FinalForward = FQuat(Axis, MaxTurnRadians).RotateVector(CurrentForward).GetSafeNormal();
			}
			else
			{
				FinalForward = CurrentForward;
			}
		}
	}

	FVector UpForRotation = FrameBinormal - FVector::DotProduct(FrameBinormal, FinalForward) * FinalForward;
	if (!UpForRotation.Normalize())
	{
		UpForRotation = FVector::UpVector - FVector::DotProduct(FVector::UpVector, FinalForward) * FinalForward;
		if (!UpForRotation.Normalize())
		{
			UpForRotation = FVector::RightVector;
		}
	}

	Projectile->SetActorRotation(FRotationMatrix::MakeFromXZ(FinalForward, UpForRotation).Rotator());

	// 飛行割合が閾値を超えたらベジェ誘導をやめて対象へホーミングへ切り替え
	if (HomingHandoffRatio > 0.0f && HomingTarget.IsValid() && t >= HomingHandoffRatio)
	{
		HomingDir = (NewPos - PrevPos).GetSafeNormal();
		if (HomingDir.IsNearlyZero())
		{
			HomingDir = FrameTangent;
		}
		HomingMoveSpeed = (NewPos - PrevPos).Size() / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
		Phase = EArcPhase::Homing;
		return;
	}

	if (t >= 1.0f)
	{
		// 終端の瞬間速度ではなくFiring中の巡航速度(平均)を慣性へ渡す
		// ループ型の制御点でも到達後に減速して見えないようにする
		const float CruiseSpeed = (FiringSpeedSampleCount > 0)
			? FiringSpeedSum / static_cast<float>(FiringSpeedSampleCount)
			: (NewPos - PrevPos).Size() / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
		const float HandoffSpeed = FMath::Max(CruiseSpeed, Projectile->InitialSpeed);
		EnterInertialFlight(Projectile, FinalForward.GetSafeNormal() * HandoffSpeed);
	}
}

FVector UBezierArcBehavior::EvaluateCubicPosition(float t) const
{
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	return uu * u * P0 + 3.0f * uu * t * P1 + 3.0f * u * tt * P2 + tt * t * P3;
}

FVector UBezierArcBehavior::EvaluateCubicTangent(float t) const
{
	const float u = 1.0f - t;
	return 3.0f * u * u * (P1 - P0) + 6.0f * u * t * (P2 - P1) + 3.0f * t * t * (P3 - P2);
}

void UBezierArcBehavior::InitializeFrame(const FVector& Tangent)
{
	FrameTangent = Tangent.GetSafeNormal();
	FVector ReferenceUp = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(FrameTangent, ReferenceUp)) > 0.98f)
	{
		ReferenceUp = FVector::ForwardVector;
	}

	FrameNormal = FVector::CrossProduct(ReferenceUp, FrameTangent).GetSafeNormal();
	if (FrameNormal.IsNearlyZero())
	{
		FrameNormal = FVector::RightVector;
	}
	FrameBinormal = FVector::CrossProduct(FrameTangent, FrameNormal).GetSafeNormal();
	bFrameInitialized = true;
}

void UBezierArcBehavior::UpdateFrame(const FVector& Tangent)
{
	const FVector NewTangent = Tangent.GetSafeNormal();
	if (NewTangent.IsNearlyZero())
	{
		return;
	}

	const float Dot = FMath::Clamp(FVector::DotProduct(FrameTangent, NewTangent), -1.0f, 1.0f);
	const FVector Axis = FVector::CrossProduct(FrameTangent, NewTangent);
	const float BombardSmallTolerance = 1.0e-4f;
	if (Axis.SizeSquared() > BombardSmallTolerance)
	{
		const FVector RotationAxis = Axis.GetSafeNormal();
		const float Angle = FMath::Acos(Dot);
		const FQuat DeltaRot(RotationAxis, Angle);
		FrameNormal = DeltaRot.RotateVector(FrameNormal).GetSafeNormal();
	}

	FrameTangent = NewTangent;
	FrameBinormal = FVector::CrossProduct(FrameTangent, FrameNormal).GetSafeNormal();
	if (FrameBinormal.IsNearlyZero())
	{
		InitializeFrame(FrameTangent);
		return;
	}
	FrameNormal = FVector::CrossProduct(FrameBinormal, FrameTangent).GetSafeNormal();
}

FVector UBezierArcBehavior::EvaluateWeaveOffset(float t) const
{
	if (WeaveNoiseAmplitude <= KINDA_SMALL_NUMBER || WeaveNoiseFrequency <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float Envelope = FMath::Pow(FMath::Sin(PI * t), FMath::Max(WeaveEnvelopePower, 1.0f));
	if (Envelope <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float uNoise = FMath::PerlinNoise1D(t * WeaveNoiseFrequency + NoiseSeedU);
	const float vNoise = FMath::PerlinNoise1D(t * WeaveNoiseFrequency + NoiseSeedV);
	return (FrameNormal * uNoise + FrameBinormal * vNoise) * (WeaveNoiseAmplitude * Envelope);
}

void UBezierArcBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	if (!Projectile || Projectile->WasHit() || Projectile->GetEndReason() != EProjectileEndReason::LifeSpanExpired)
	{
		return;
	}

	if (SplashRadius <= KINDA_SMALL_NUMBER || SplashDamage <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = Projectile->GetOwner();
	const FVector Center = Projectile->GetActorLocation();

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(Projectile, Center, SplashRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn),
		  UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) },
		nullptr, { OwnerActor }, OverlapActors);

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
