// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "ContactCameraShakeBehavior.generated.h"

/**
 * 接触/着弾時のカメラシェイク
 * 弾とプレイヤーの距離でシェイク強度を決める
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "接触/着弾カメラシェイク"))
class PRJ_TIDE_P0_API UContactCameraShakeBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 再生するカメラシェイク
	UPROPERTY(EditAnywhere)
	TSubclassOf<class UCameraShakeBase> CameraShake;

	// この距離より近いと強度が最大になる
	UPROPERTY(EditAnywhere)
	float NearDistance = 2000.0f;

	// この距離より遠いと強度が0になる
	UPROPERTY(EditAnywhere)
	float FarDistance = 6000.0f;

	// 直接ヒットで発火
	UPROPERTY(EditAnywhere)
	bool bOnHit = true;

	// ヒットせず消滅したときに発火
	UPROPERTY(EditAnywhere)
	bool bOnExpire = false;

	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;

private:

	void PlayShake(AEnemyProjectile* Projectile) const;

};
