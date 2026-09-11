// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "GuidanceBehavior.generated.h"

/**
 * 誘導弾 (追尾ミサイル)
 *
 * 目標 (アクタor固定点) へ向け、旋回レート制限付きの追尾で飛ぶ誘導制御。
 * launch直後は初期方位を一定時間保持してから追尾に入り、サーカス的な曲線を描く。
 * 旋回レートを下げるほど大きく回り込む。
 *
 * 移動は自前でスウィープ駆動しProjectileMovementは止める (ベジェ弧と同じ作法)。
 * 炸裂は次のいずれか先に起きた時点: 接触 (OnHit) / 近接信管 / 寿命切れ (MaxLifeTime)。
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "誘導弾 (追尾)"))
class PRJ_TIDE_P0_API UGuidanceBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 巡航速度
	UPROPERTY(EditAnywhere, Category = "Guidance", meta = (ClampMin = "0.0"))
	float Speed = 1500.0f;

	// 加速度 発射後にぐんぐん伸びる演出 ※0で等速
	UPROPERTY(EditAnywhere, Category = "Guidance")
	float Acceleration = 0.0f;

	// 速度上限 ※0で無制限
	UPROPERTY(EditAnywhere, Category = "Guidance", meta = (ClampMin = "0.0"))
	float MaxSpeed = 0.0f;

	// 最大旋回レート (deg/s) 小さいほど大きく回り込み追尾が緩くなる
	UPROPERTY(EditAnywhere, Category = "Guidance", meta = (ClampMin = "0.0"))
	float TurnRateDegPerSec = 240.0f;

	// 発射直後に初期方位を保持する秒数
	UPROPERTY(EditAnywhere, Category = "Guidance", meta = (ClampMin = "0.0"))
	float HeadingHoldTime = 0.15f;

	// 目標の中心へ狙う際のカプセル高さ係数 (0 = 足元, 0.5 = 中心付近, 1.0 = 頭)
	UPROPERTY(EditAnywhere, Category = "Guidance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetCenterHeightFraction = 0.5f;

	// 近接信管半径 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Guidance|Fuse", meta = (ClampMin = "0.0"))
	float ProximityFuseRadius = 0.0f;

	// 炸裂ダメージ半径
	UPROPERTY(EditAnywhere, Category = "Guidance|Splash")
	float SplashRadius = 200.0f;

	// 炸裂ダメージ
	UPROPERTY(EditAnywhere, Category = "Guidance|Splash")
	float SplashDamage = 20.0f;

	// --- 1発ごとに技側が注入する (発射前にFindBehavior経由で呼ぶ) ---
	void SetTarget(AActor* InTarget);
	void SetTargetLocation(const FVector& WorldLocation);
	void SetInitialHeadingDirection(const FVector& WorldDirection);

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual void OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;
	virtual bool WantsTick() const override { return true; }

private:

	// 目標の狙い点 (中心高さ補正済み)。アクタが無効なら固定点を返す
	FVector ResolveAimCenter() const;
	bool MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation);
	void Detonate(AEnemyProjectile* Projectile, const FVector& Center, AActor* DirectHitActor);

	// ---- 1発ごとの設定 (OnLaunchで取り込み後リセット) ----
	TWeakObjectPtr<AActor> CfgTarget;
	FVector CfgTargetPoint = FVector::ZeroVector; bool bCfgTargetPoint = false;
	FVector CfgHeadingDir = FVector::ZeroVector; bool bCfgHeading = false;

	// ---- 実行時状態 (OnLaunchで初期化) ----
	TWeakObjectPtr<AActor> TargetActor;
	FVector TargetPoint = FVector::ZeroVector;
	bool bHasTargetPoint = false;

	FVector Dir = FVector::ForwardVector;
	float CurrentSpeed = 0.0f;
	float Elapsed = 0.0f;
	bool bDetonated = false;

	FVector InitialHeadingDirection = FVector::ForwardVector;
	bool bInitialHeadingSet = false;

};
