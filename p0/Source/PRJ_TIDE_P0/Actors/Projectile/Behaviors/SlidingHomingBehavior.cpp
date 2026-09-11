// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "SlidingHomingBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// OldVelからNewVelへの向き変化をMaxTurnRadians以内に収める 速さはNewVel側を保つ
	FVector ClampTurn(const FVector& OldVel, const FVector& NewVel, float MaxTurnRadians)
	{
		const FVector OldDir = OldVel.GetSafeNormal();
		const FVector NewDir = NewVel.GetSafeNormal();
		if (OldDir.IsNearlyZero() || NewDir.IsNearlyZero()) return NewVel;

		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(OldDir, NewDir), -1.0f, 1.0f));
		if (Angle <= MaxTurnRadians) return NewVel;

		FVector Axis = FVector::CrossProduct(OldDir, NewDir);
		// 真逆で軸が定まらない場合はヨー方向へ回して減速で止まるのを避ける
		if (!Axis.Normalize()) Axis = FVector::UpVector;

		return OldDir.RotateAngleAxis(FMath::RadiansToDegrees(MaxTurnRadians), Axis) * NewVel.Size();
	}
}

void USlidingHomingBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	Elapsed = 0.0f;
	bHomingStarted = false;
	bHomingActive = false;

	if (!Projectile || !Projectile->ProjectileMovement) return;

	UProjectileMovementComponent* PMC = Projectile->ProjectileMovement;

	// 浅い斜面では消滅させず斜面に沿って滑らせる
	// Bounciness=0 +
	// Friction=0でめり込み成分だけ除去し接線速度は保持する跳ね返りになる
	PMC->bShouldBounce = true;
	PMC->Bounciness = 0.0f;
	PMC->Friction = 0.0f;
	PMC->bBounceAngleAffectsFriction = false;

	// 地形に沿って滑走するためWorldDynamicは素通りさせる
	if (Projectile->CollisionComp)
	{
		Projectile->CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	}

	// 追従はOnTickの手動処理で行う 内蔵ホーミングは旋回角を制限できないので使わない
	PMC->bIsHomingProjectile = false;
	if (HomingAccelerationMagnitude > 0.0f)
	{
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(Projectile, 0))
		{
			PMC->HomingTargetComponent = Player->GetRootComponent();
		}
	}
}

void USlidingHomingBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile || !Projectile->ProjectileMovement) return;
	UProjectileMovementComponent* PMC = Projectile->ProjectileMovement;

	Elapsed += DeltaSeconds;

	// 開始遅延の経過でホーミングを有効化する
	if (!bHomingStarted && HomingAccelerationMagnitude > 0.0f && Elapsed >= HomingStartDelay)
	{
		if (PMC->HomingTargetComponent.IsValid())
		{
			bHomingStarted = true;
			bHomingActive = true;
		}
	}

	if (!bHomingActive) return;

	// 持続時間で打ち切り
	if (HomingDuration > 0.0f && (Elapsed - HomingStartDelay) > HomingDuration)
	{
		DisableHoming();
		return;
	}

	if (bDisableHomingOnPassby)
	{
		const USceneComponent* Target = PMC->HomingTargetComponent.Get();
		if (Target)
		{
			// 内積が負ですれ違いと判定
			// 追従面に揃える 3Dのままだと落下中のZ成分が正に効き近距離ですれ違いを見逃す
			FVector ToTarget = Target->GetComponentLocation() - Projectile->GetActorLocation();
			FVector Vel = PMC->Velocity;
			if (!bHomingVertical)
			{
				ToTarget.Z = 0.0f;
				Vel.Z = 0.0f;
			}

			if (FVector::DotProduct(Vel, ToTarget) < 0.0f)
			{
				DisableHoming();
			}
		}
	}

	// 加速度で向きを寄せてから1フレームぶんの旋回角で頭打ちにする
	// bHomingVerticalがfalseならXYだけ動かし、垂直は重力/地形に委ねる
	if (bHomingActive)
	{
		if (const USceneComponent* Target = PMC->HomingTargetComponent.Get())
		{
			FVector ToTarget = Target->GetComponentLocation() - Projectile->GetActorLocation();
			if (!bHomingVertical) ToTarget.Z = 0.0f;

			const FVector Dir = ToTarget.GetSafeNormal();
			if (!Dir.IsNearlyZero())
			{
				const FVector OldVel = bHomingVertical ? PMC->Velocity : FVector(PMC->Velocity.X, PMC->Velocity.Y, 0.0f);
				FVector NewVel = OldVel + Dir * HomingAccelerationMagnitude * DeltaSeconds;
				if (MaxTurnRateDeg > 0.0f)
				{
					NewVel = ClampTurn(OldVel, NewVel, FMath::DegreesToRadians(MaxTurnRateDeg) * DeltaSeconds);
				}
				PMC->Velocity = bHomingVertical ? NewVel : FVector(NewVel.X, NewVel.Y, PMC->Velocity.Z);
			}
		}
	}
}

bool USlidingHomingBehavior::ShouldConsumeOnHit(AEnemyProjectile* Projectile, const FHitResult& Hit, bool bDefault)
{
	// 衝突面の傾斜角(水平からの角度) 平地=0度 垂直な壁=90度
	// しきい値以内は消滅させず滑走(false) 超える急斜面・壁は消滅(true)
	const float UpDot = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector);
	const float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(UpDot, -1.0f, 1.0f)));
	return SlopeAngle > MaxSlideAngle;
}

void USlidingHomingBehavior::DisableHoming()
{
	bHomingActive = false;
}
