// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "HomingProjectileBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

void UHomingProjectileBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// プール再利用に備え実行時状態をすべて初期化
	Elapsed = 0.0f;
	SteerElapsed = 0.0f;
	ActiveTurnRate = 0.0f;
	bBegun = false;
	bSteering = false;

	Target = nullptr;

	if (!Projectile) return;

	// 追従対象はプレイヤーPawnを自動取得する
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(Projectile, 0))
	{
		Target = Player->GetRootComponent();
	}
}

void UHomingProjectileBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile || !Projectile->ProjectileMovement) return;

	USceneComponent* TargetComp = Target.Get();
	if (!TargetComp) return;

	Elapsed += DeltaSeconds;
	if (Elapsed < BeginDelay) return; // 追従開始前は直進する

	if (!bBegun)
	{
		bBegun = true;
		bSteering = true;
		SteerElapsed = 0.0f;

		// この弾の旋回速度を[Min, Max]から抽選して確定 ※弾ごとにばらつく
		const float RateMin = FMath::Min(TurnRateMinDegPerSec, TurnRateMaxDegPerSec);
		const float RateMax = FMath::Max(TurnRateMinDegPerSec, TurnRateMaxDegPerSec);
		ActiveTurnRate = FMath::FRandRange(RateMin, RateMax);
	}

	if (!bSteering) return; // 打ち切り後は速度を維持して直進する

	SteerElapsed += DeltaSeconds;
	if (ActiveDuration > 0.0f && SteerElapsed > ActiveDuration)
	{
		bSteering = false; // 一定時間で追従を打ち切って回避猶予を作る
		return;
	}

	const FVector ToTarget = TargetComp->GetComponentLocation() - Projectile->GetActorLocation();

	// 近距離まで詰めたら追従を打ち切り残りは直進させて回避猶予を作る
	if (StopDistance > 0.0f && ToTarget.SizeSquared() <= FMath::Square(StopDistance))
	{
		bSteering = false;
		return;
	}

	// 旋回速度制限ホーミング
	const FVector CurVel = Projectile->ProjectileMovement->Velocity;
	const float Speed = CurVel.Size();
	const FVector DesiredDir = ToTarget.GetSafeNormal();
	if (Speed < KINDA_SMALL_NUMBER || DesiredDir.IsNearlyZero()) return;

	const FVector CurDir = CurVel / Speed;

	// 方向をyawとpitchに分解高台から発射されたとき地面に衝突しないように
	auto ToYawPitch = [](const FVector& D, float& OutYaw, float& OutPitch)
		{
			OutYaw = FMath::Atan2(D.Y, D.X);
			OutPitch = FMath::Atan2(D.Z, FVector(D.X, D.Y, 0.0f).Size());
		};

	float CurYaw, CurPitch, DesYaw, DesPitch;
	ToYawPitch(CurDir, CurYaw, CurPitch);
	ToYawPitch(DesiredDir, DesYaw, DesPitch);

	// 残り寿命に比例してホーミング性能を落としてみる
	float LifeScale = 1.0f;
	if (Projectile->MaxLifeTime > KINDA_SMALL_NUMBER)
	{
		const float LifeFrac = FMath::Clamp(Projectile->GetLifeSpan() / Projectile->MaxLifeTime, 0.0f, 1.0f);
		LifeScale = FMath::Lerp(FMath::Clamp(DecayMinScale, 0.0f, 1.0f), 1.0f, LifeFrac);
	}

	const float MaxYawRad = FMath::DegreesToRadians(ActiveTurnRate) * LifeScale * DeltaSeconds;
	const float MaxPitchRad = FMath::DegreesToRadians(PitchTurnRateDegPerSec) * LifeScale * DeltaSeconds;

	// ヨーは最短回りピッチは[-90,90]なので素直に差分をクランプ
	const float DeltaYaw = FMath::Clamp(FMath::FindDeltaAngleRadians(CurYaw, DesYaw), -MaxYawRad, MaxYawRad);
	const float DeltaPitch = FMath::Clamp(DesPitch - CurPitch, -MaxPitchRad, MaxPitchRad);

	const float NewYaw = CurYaw + DeltaYaw;
	const float NewPitch = CurPitch + DeltaPitch;
	const float CosP = FMath::Cos(NewPitch);
	const FVector NewDir(CosP * FMath::Cos(NewYaw), CosP * FMath::Sin(NewYaw), FMath::Sin(NewPitch));

	Projectile->ProjectileMovement->Velocity = NewDir.GetSafeNormal() * Speed;
}
