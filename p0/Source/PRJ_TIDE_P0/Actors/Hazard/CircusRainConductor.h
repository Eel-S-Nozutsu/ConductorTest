// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CircusRainConductor.generated.h"

class AEnemyCharacter;
class AEnemyProjectile;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * サーカス一斉降雨の6秒間を担保する使い捨てコンダクター。
 *
 * 降雨は最初のShotで全弾を上空へ打ち上げてホバー保持させ、その時点でAttackExecutionは攻撃を
 * 終える (ボスは次の攻撃へ)。攻撃が消えた後の「各弾を時間差で降下させる / 降下開始時にPCを
 * 再サンプルして着弾点を確定する / 予兆デカールを進める」をこのActorが肩代わりする。
 *
 * AddDescentで積んだ弾を、それぞれの降下時刻にPCの現在位置基準の着弾点へ撃ち下ろし
 * (UArcPathBehavior::CommandDescend)、同時に予兆デカールを出す。全弾の降下を出し切り、かつ
 * デカールも消化しきったら自分ごとDestroyする。
 */
UCLASS()
class PRJ_TIDE_P0_API ACircusRainConductor : public AActor
{
	GENERATED_BODY()

public:

	// 降下の形と予兆デカールの設定。AttackExecutionから値でコピーして渡す
	struct FConfig
	{
		// 着弾点を散らすPC中心の半径
		float ScatterRadius = 800.0f;
		// PC速度から何秒先を予測して着弾円の中心に置くか
		float PredictLeadTime = 0.3f;
		// 降下 (ホバー → 着弾) の飛行秒数 = 予兆デカールの表示時間
		float DescentFlightDuration = 1.6f;
		// 降下の中間制御点を持ち上げる量。0で最短降下、大きいほど大きく回り込む
		float DescentSwoopHeight = 150.0f;
		// 着弾点の真上へ置く制御点の高さ。大きいほど垂直に近い角度で落ちる
		float ImpactApproachHeight = 800.0f;

		// 着弾Zを合わせる下方向トレースの上端/距離
		float GroundTraceUp = 1000.0f;
		float GroundTraceDown = 4000.0f;

		// 予兆デカール (未設定なら予兆なし)
		TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;
		FName DecalProgressParam = TEXT("Progress");
		float DecalRadius = 200.0f;
		float DecalDepth = 200.0f;

		bool bDebugDraw = false;

	};

	// 発射元と追尾対象、降下/デカール設定を渡して起動する。寿命の保険もここで張る
	void Initialize(AEnemyCharacter* InEnemy, const FConfig& InConfig, float SafetyLifeSpan);

	// ホバー中の弾を1発、DescendAtTime (起動からの秒数) に降下させるよう予約する
	void AddDescent(AEnemyProjectile* Missile, float DescendAtTime);

	ACircusRainConductor();

protected:

	virtual void Tick(float DeltaTime) override;

private:

	// 予約した弾を降下させ、着弾点へ予兆デカールを出す
	void CommandDescent(AEnemyProjectile* Missile);
	// XYはそのままに真下の床へZを落とす
	FVector ProjectToGround(const FVector& Location) const;
	// 着弾点に予兆デカールを出し、
	// DescentFlightDurationかけてProgressを0->1で回す
	void AddImpactDecal(const FVector& Location);
	// 予兆デカールの進行と着弾破棄
	void UpdateDecals(float DeltaTime);

	// 1発ぶんの降下予約
	struct FScheduledDescent
	{
		TWeakObjectPtr<AEnemyProjectile> Missile;
		float DescendAtTime = 0.0f;
		bool bCommanded = false;

	};

	// 着弾までの予兆デカール1つぶん
	struct FImpactDecal
	{
		float RemainingTime = 0.0f;
		float TotalTime = 0.0f;
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UMaterialInstanceDynamic> DecalMID;

	};

	TWeakObjectPtr<AEnemyCharacter> Enemy;
	FConfig Config;

	TArray<FScheduledDescent> Descents;
	TArray<FImpactDecal> PendingDecals;

	float Elapsed = 0.0f;
	int32 CommandedCount = 0;

};
