// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "BezierArcBehavior.generated.h"

/**
 * 着弾位置指定型のサーカス弾道
 *
 * 3次ベジェ弧で着弾点へ飛びFlightDuration秒で到達して範囲爆発
 * 散弾/弧状/扇状の3攻撃で共有 動きは同一で着弾点/制御点/展開先/ハンドオフを技側が注入
 *
 * 展開/待機/照準/本発射/ホーミング/落下
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "ベジェ弧 (着弾点へ)"))
class PRJ_TIDE_P0_API UBezierArcBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// ---- 飛行パラメータ ----

	// 制御点未指定時の弧の高さ
	UPROPERTY(EditAnywhere, Category = "Arc")
	float ArcHeight = 600.0f;

	// 着弾までの飛行秒数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float FlightDuration = 1.5f;

	// 寿命切れ時の空中爆発半径
	UPROPERTY(EditAnywhere, Category = "Arc")
	float SplashRadius = 200.0f;

	// 寿命切れ時の空中爆発ダメージ量
	UPROPERTY(EditAnywhere, Category = "Arc")
	float SplashDamage = 20.0f;

	// 蛇行ノイズの最大振幅 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Arc")
	float WeaveNoiseAmplitude = 90.0f;

	// 蛇行ノイズの周波数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float WeaveNoiseFrequency = 3.0f;

	// 蛇行ノイズの振幅変化のべき乗指数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float WeaveEnvelopePower = 2.0f;

	// 射出直後に初期方位を維持する秒数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float HeadingHoldTime = 0.3f;

	// 姿勢が目標方位へ向く最大旋回速度(deg/s)
	UPROPERTY(EditAnywhere, Category = "Arc")
	float TurnRateDegPerSec = 420.0f;

	// ---- 展開(SetDeploy使用時)----

	// 展開先までの飛行速度
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeploySpeed = 1500.0f;

	// 展開先への許容距離
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployAcceptRadius = 50.0f;

	// 展開中の上下揺れの振幅 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployHoverBobAmplitude = 12.0f;

	// ホーミング打ち切り保険の最大秒数
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float HomingMaxDuration = 3.0f;

	// 待機後に対象へ向き直す秒数 ※0で即発射
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float AimDuration = 0.5f;

	// 待機後に対象へ向き直す最大旋回速度 (deg/s)
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float AimRotationRate = 360.0f;

	// ---- 1発ごとに技側が注入する動的パラメータ
	// (発射前にFindBehavior経由で呼ぶ)----
	void SetTargetLocation(const FVector& WorldLocation);
	void SetControlPoint(const FVector& WorldLocation); // 単一ガイド点
	void SetControlPoints(const FVector& InP1, const FVector& InP2); // 3次ベジェ制御点
	void SetInitialHeadingDirection(const FVector& WorldDirection);
	void SetDeploy(const FVector& InDeployLocation, float InHoverDuration);
	void SetHomingHandoff(AActor* InTarget, float InHandoffRatio, float InTurnRateDegPerSec);

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;
	virtual bool WantsTick() const override { return true; }

private:

	FVector EvaluateCubicPosition(float t) const;
	FVector EvaluateCubicTangent(float t) const;
	void InitializeFrame(const FVector& Tangent);
	void UpdateFrame(const FVector& Tangent);
	FVector EvaluateWeaveOffset(float t) const;
	bool MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation);
	void EnterInertialFlight(AEnemyProjectile* Projectile, const FVector& InitialVelocity);

	enum class EArcPhase : uint8 { Deploying, Hovering, Aiming, Firing, Homing, Inertial };
	void StartFiringFrom(const FVector& Origin);
	void TickDeploy(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickHover(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickAim(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickFiring(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickHoming(AEnemyProjectile* Projectile, float DeltaSeconds);
	FVector HomingTargetCenter(AEnemyProjectile* Projectile) const;

	// ---- 1発ごとの設定(Set* で書き込み、
	// OnLaunchで実行時状態へ取り込み後リセット)----
	FVector CfgP3 = FVector::ZeroVector; bool bCfgTarget = false;
	FVector CfgP1 = FVector::ZeroVector;
	FVector CfgP2 = FVector::ZeroVector; bool bCfgControl = false; bool bCfgDual = false;
	FVector CfgHeadingDir = FVector::ZeroVector; bool bCfgHeading = false;
	FVector CfgDeployLoc = FVector::ZeroVector; float CfgHoverDur = 0.0f; bool bCfgDeploy = false;
	TWeakObjectPtr<AActor> CfgHomingTarget; float CfgHandoffRatio = 0.0f; float CfgHandoffTurn = 360.0f;

	// ---- 実行時状態(OnLaunchで初期化)----
	EArcPhase Phase = EArcPhase::Firing;

	bool bUseDeploy = false;
	FVector DeployLocation = FVector::ZeroVector;
	float DeployHoverDuration = 0.0f;
	FVector HoverBaseLocation = FVector::ZeroVector;
	float HoverElapsed = 0.0f;
	float AimElapsed = 0.0f;
	float HomingElapsed = 0.0f;

	TWeakObjectPtr<AActor> HomingTarget;
	float HomingHandoffRatio = 0.0f;
	float HomingHandoffTurnRateDegPerSec = 360.0f;
	float HomingMoveSpeed = 0.0f;
	FVector HomingDir = FVector::ForwardVector;

	FVector P0 = FVector::ZeroVector;
	FVector P1 = FVector::ZeroVector;
	FVector P2 = FVector::ZeroVector;
	FVector P3 = FVector::ZeroVector;
	float Elapsed = 0.0f;
	bool bCustomControlPoint = false;
	bool bDualControlPoints  = false;

	// Firing中のステップ速度の累計とサンプル数。終端ハンドオフで巡航速度(平均)を求めるのに使う
	// ベジェ終端の瞬間速度は制御点配置で巡航より遅くなり得るため、平均を使って到達後の減速を防ぐ
	float FiringSpeedSum = 0.0f;
	int32 FiringSpeedSampleCount = 0;

	FVector FrameTangent = FVector::ForwardVector;
	FVector FrameNormal = FVector::RightVector;
	FVector FrameBinormal = FVector::UpVector;
	bool bFrameInitialized = false;

	float NoiseSeedU = 0.0f;
	float NoiseSeedV = 0.0f;

	FVector InitialHeadingDirection = FVector::ZeroVector;
	bool bInitialHeadingSet = false;

};
