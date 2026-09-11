// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "ArcPathBehavior.generated.h"

/**
 * アート指定の弧を等速で描くスプライン弾道 (弧長等速ベジェ + 誘導テール)
 *
 * 3次ベジェ弧を弧長でパラメータ化して等速に走らせる。linear-t由来の終端減速が起きず、
 * ホーミング/慣性へのハンドオフも巡航速度のまま継ぎ目なく渡せる。
 * 横ふくらみ等「弧の形を指定したい」演出弧に使う (弧状爆撃)。
 *
 * 展開/待機/照準/本発射(弧)/ホーミング/慣性 のフェーズを持ち、着弾点/制御点/展開先/
 * ハンドオフを技側が1発ごとに注入する。APIはUBezierArcBehaviorと同一。
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "弧パス (弧長等速ベジェ)"))
class PRJ_TIDE_P0_API UArcPathBehavior : public UProjectileBehavior
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

	// 蛇行ノイズの周波数 (Hz)
	UPROPERTY(EditAnywhere, Category = "Arc")
	float WeaveNoiseFrequency = 3.0f;

	// 蛇行ノイズの振幅変化のべき乗指数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float WeaveEnvelopePower = 2.0f;

	// 揺らぎ (ノイズ/螺旋) を収束させ始める飛行割合 (0..1)
	// ここから着弾までで滑らかにゼロへ落とす。1.0で無効 (中央が山の対称な包絡のまま)
	// 0.6なら後半4割は素直な弾道になり、着弾直前だけ落ち着いて見える。
	// 射出直後の暴れ具合には影響しない
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaveSettleRatio = 1.0f;

	// 螺旋の回転数 (飛行全体で何周するか) 0で螺旋なし
	// ノイズが不規則にふらつくのに対し、こちらは規則的にカールする (アニメのミサイル表現)
	UPROPERTY(EditAnywhere, Category = "Arc")
	float SpiralTurns = 0.0f;

	// 螺旋の半径 0で無効。包絡はノイズと共通なので始点/終点では必ずゼロになる
	UPROPERTY(EditAnywhere, Category = "Arc")
	float SpiralAmplitude = 0.0f;

	// 螺旋の回る向きを弾ごとにランダム反転する。一斉発射で全弾が同じ巻き方をしないように
	UPROPERTY(EditAnywhere, Category = "Arc")
	bool bRandomizeSpiralDirection = true;

	// 射出直後に初期方位を維持する秒数
	UPROPERTY(EditAnywhere, Category = "Arc")
	float HeadingHoldTime = 0.3f;

	// 姿勢が目標方位へ向く最大旋回速度 (deg/s)
	UPROPERTY(EditAnywhere, Category = "Arc")
	float TurnRateDegPerSec = 420.0f;

	// ---- 展開 (SetDeploy使用時) ----

	// 展開先までの飛行速度
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeploySpeed = 1500.0f;

	// 展開先への許容距離
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployAcceptRadius = 50.0f;

	// 展開中の上下揺れの振幅 ※0で無効
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float DeployHoverBobAmplitude = 12.0f;

	// ホーミング打ち切り保険の秒数
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float HomingMaxDuration = 3.0f;

	// 待機後に対象へ向き直す秒数 ※0で即発射
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float AimDuration = 0.5f;

	// 待機後に対象へ向き直す最大旋回速度 (deg/s)
	UPROPERTY(EditAnywhere, Category = "Deploy")
	float AimRotationRate = 360.0f;

	// ---- デバッグ ----

	// 弾道を可視化する。発射時に制御点と基準ベジェ (揺らぎ無し) を描き、
	// 飛行中は実際に通った線を描く
	// 基準線と実際の線の差が、螺旋とノイズが乗せているぶん
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugDrawPath = false;

	// ---- 1発ごとに技側が注入する動的パラメータ (発射前にFindBehavior経由で呼ぶ)
	// ----
	void SetTargetLocation(const FVector& WorldLocation);
	void SetControlPoint(const FVector& WorldLocation); // 単一ガイド点
	void SetControlPoints(const FVector& InP1, const FVector& InP2); // 3次ベジェ制御点
	// 任意個の中間制御点 (始点と着弾点は含めない)。次数 = 個数 + 1のベジェになる
	// 3次では「登りは直進 / 頂点で回す / 降りは直進」のように回転を区間へ配分できないため、
	// 上げて落とす弾道など向きが大きく変わる軌道はここで点を増やして作る
	void SetCurveMidPoints(const TArray<FVector>& InMidPoints);
	void SetInitialHeadingDirection(const FVector& WorldDirection);
	void SetDeploy(const FVector& InDeployLocation, float InHoverDuration);
	void SetHomingHandoff(AActor* InTarget, float InHandoffRatio, float InTurnRateDegPerSec);

	// 展開/待機 (Deploying/Hovering/Aiming) 中の弾を、
	// 外部の指令で「今から」本発射(弧)へ移す
	// 打ち上げてホバー保持した弾を、降下開始のその瞬間に着弾点を確定して撃ち下ろすための入口
	// CurrentLocationを弧の始点にし、Target/MidPoints
	// /FlightDurationを差し替えて再構築する。既に発射(Firing)以降なら何もしない
	void CommandDescend(const FVector& CurrentLocation, const FVector& Target,
		const TArray<FVector>& InMidPoints, float InFlightDuration);

	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual void OnExpire(AEnemyProjectile* Projectile) override;
	virtual bool WantsTick() const override { return true; }

private:

	// 任意次数のベジェ (de Casteljau)
	// CurvePoints = 始点 + 中間制御点 + 着弾点
	FVector EvaluatePosition(float t) const;
	FVector EvaluateTangent(float t) const;
	// 始点と中間制御点からCurvePointsを組み立てる
	// (中間点未指定ならArcHeightで山なりを作る)
	void BuildCurve(const FVector& Start);
	void InitializeFrame(const FVector& Tangent);
	void UpdateFrame(const FVector& Tangent);
	FVector EvaluateWeaveOffset(float t) const;
	bool MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation);
	void EnterInertialFlight(AEnemyProjectile* Projectile, const FVector& InitialVelocity);

	// 弧長テーブルを構築し、巡航速度を確定する (制御点確定後に呼ぶ)
	void BuildArcLengthTable();
	// 弧長Distに対応する曲線パラメータt (テーブル線形補間)
	float ParamAtDistance(float Dist) const;

	// 制御点と基準ベジェを描く (制御点確定後に呼ぶ)
	void DebugDrawPath(const UWorld* World) const;

	enum class EArcPhase : uint8 { Deploying, Hovering, Aiming, Firing, Homing, Inertial };
	void StartFiringFrom(const FVector& Origin);
	void TickDeploy(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickHover(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickAim(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickFiring(AEnemyProjectile* Projectile, float DeltaSeconds);
	void TickHoming(AEnemyProjectile* Projectile, float DeltaSeconds);
	FVector HomingTargetCenter(AEnemyProjectile* Projectile) const;

	// ---- 1発ごとの設定 (Set* で書き込み、
	// OnLaunchで実行時状態へ取り込み後リセット) ----
	FVector CfgP3 = FVector::ZeroVector; bool bCfgTarget = false;
	TArray<FVector> CfgMidPoints; bool bCfgSingleGuide = false;
	FVector CfgHeadingDir = FVector::ZeroVector; bool bCfgHeading = false;
	FVector CfgDeployLoc = FVector::ZeroVector; float CfgHoverDur = 0.0f; bool bCfgDeploy = false;
	TWeakObjectPtr<AActor> CfgHomingTarget; float CfgHandoffRatio = 0.0f; float CfgHandoffTurn = 360.0f;

	// ---- 実行時状態 (OnLaunchで初期化) ----
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

	// 着弾点 (曲線の終点)。中断判定や終端スナップで参照するため別に持つ
	FVector P3 = FVector::ZeroVector;

	// 技側から渡された中間制御点。展開後の再発射でも同じ点を使い回す
	TArray<FVector> MidPoints;
	bool bSingleGuide = false;

	// 実際に評価する制御点列 (始点 + MidPoints + 着弾点)
	TArray<FVector> CurvePoints;

	// 弧長等速の実行時状態
	TArray<float> ArcParamTable;   // サンプルt値 (0..1)
	TArray<float> ArcLengthTable;  // 各サンプルまでの累積弧長
	float TotalArcLength = 0.0f;
	float FiringSpeed = 0.0f;      // 巡航速度 = TotalArcLength / FlightDuration
	float FiringDistance = 0.0f;   // これまでに進んだ弧長
	float FiringElapsed = 0.0f;    // Firing経過時間 (初期方位保持の判定用)

	FVector FrameTangent = FVector::ForwardVector;
	FVector FrameNormal = FVector::RightVector;
	FVector FrameBinormal = FVector::UpVector;
	bool bFrameInitialized = false;

	float NoiseSeedU = 0.0f;
	float NoiseSeedV = 0.0f;
	float SpiralPhase = 0.0f;
	float SpiralSign = 1.0f;

	FVector InitialHeadingDirection = FVector::ZeroVector;
	bool bInitialHeadingSet = false;

};
