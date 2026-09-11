// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "DeployHomingBehavior.generated.h"

/**
 * 展開ホーミング (ファンネル型)
 *
 * 1. Deploying: 後方初速で射出しDeployTurnRateで旋回しながら展開先へ飛行機のように向かう
 * 2. Hovering: HoverDuration秒ふわふわ待機 (sin波で上下)
 * 3. Aiming: AimDuration秒かけてPC方向へ旋回
 * 4. Flying: ProjectileMovement内蔵ホーミングで飛翔 時間/距離の上限で直進
 *
 * 展開先は技側がSetDeployTarget()で注入する (FindBehavior経由)
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "展開ホーミング"))
class PRJ_TIDE_P0_API UDeployHomingBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// ---- Deploy ----

	// SetDeployTarget未使用時に前方どれだけ先を目標にするか
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployDistance = 300.0f;

	// Deploy中の飛行速度
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeploySpeed = 600.0f;

	// 速度方向を目標へ向ける旋回速度
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployTurnRate = 180.0f;

	// 目標に到着とみなす距離
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployAcceptRadius = 40.0f;

	// ---- Hover ----

	// 
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverDuration = 0.8f;

	// 
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverBobAmplitude = 15.0f;

	// 
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverBobFrequency = 1.5f;

	// ---- Aim ----

	// 目標へ向かって旋回する秒数
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimDuration = 0.5f;

	// 目標へ向かって旋回する速度
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimRotationRate = 360.0f;

	// ---- Fly ----

	// 
	UPROPERTY(EditAnywhere, Category = "Fly")
	float HomingAcceleration = 3000.0f;

	// 追従を停止するまでの秒数 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Fly")
	float HomingDuration = 2.0f;

	// 追従を停止するスポーン地点からの移動距離 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Fly")
	float HomingMaxDistance = 0.0f;

	// 展開先をワールド座標で指定 ※発射前に技側がFindBehavior経由で呼ぶ
	void SetDeployTarget(const FVector& WorldLocation);

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual bool WantsTick() const override { return true; }

private:

	void Launch(AEnemyProjectile* Projectile);
	void StopHoming(AEnemyProjectile* Projectile);

	enum class EHomingPhase : uint8
	{
		Deploying,
		Hovering,
		Aiming,
		Flying
	};

	EHomingPhase Phase = EHomingPhase::Deploying;
	TWeakObjectPtr<class ACharacter> CachedPlayer;

	FVector DeployTargetLocation = FVector::ZeroVector;
	bool bCustomDeployTarget = false; // SetDeployTargetで設定済みか
	FVector DeployVelocity = FVector::ZeroVector;

	FVector HoverBaseLocation = FVector::ZeroVector;
	float HoverElapsed = 0.0f;
	float AimElapsed = 0.0f;

	FVector SpawnLocation = FVector::ZeroVector;
	float HomingElapsed = 0.0f;
	bool bHoming = false;

};
