// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContactCameraShakeBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

void UContactCameraShakeBehavior::OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result)
{
	if (bOnHit)
	{
		PlayShake(Projectile);
	}
}

void UContactCameraShakeBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	// ヒットして消えた弾はOnHit側で発火済みなので 未ヒット消滅のみ対象にする
	if (bOnExpire && Projectile && !Projectile->WasHit())
	{
		PlayShake(Projectile);
	}
}

void UContactCameraShakeBehavior::PlayShake(AEnemyProjectile* Projectile) const
{
	if (!Projectile || !CameraShake) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(Projectile, 0);
	if (!PC) return;
	APlayerCameraManager* CameraManager = PC->PlayerCameraManager;
	if (!CameraManager) return;

	const FVector Center = Projectile->GetActorLocation();
	const APawn* Pawn = PC->GetPawn();
	const FVector CamLocation = CameraManager->GetCameraLocation();
	const FVector PcLocation = Pawn ? Pawn->GetActorLocation() : CamLocation;
	const float Dist = FVector::Distance(PcLocation, Center);

	const float Near = FMath::Min(NearDistance, FarDistance);
	const float Far = FMath::Max(NearDistance, FarDistance);
	const float Range = FMath::Max(Far - Near, KINDA_SMALL_NUMBER);
	const float ShakeT = FMath::Clamp((Far - Dist) / Range, 0.0f, 1.0f);
	const float ShakeScaleRate = FMath::SmoothStep(0.0f, 1.0f, ShakeT);

	if (ShakeScaleRate > KINDA_SMALL_NUMBER)
	{
		CameraManager->StartCameraShake(CameraShake, ShakeScaleRate);
	}
}
