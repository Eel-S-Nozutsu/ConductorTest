// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BossShockwaveActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * ボスのショックウェーブ攻撃アクター (プロシージャルメッシュ版)
 *
 * 中心から広がるリング (アニュラス) をプロシージャルメッシュで毎フレーム生成する。
 * - リングの半径方向の幅はRingThickness固定。半径が増えても太さが変わらない
 * - 各セグメントを下方向トレースして地面の高さに合わせる (非平坦な釜に沿う)
 * - ダメージ/光輪破砕は中心からの2D半径バンド判定で行う (メッシュコリジョンは使わない)
 *
 * Phase 1 (Rising):    出現待機 (InitialRadiusのリングを表示)
 * Phase 2 (Holding):   拡大開始までの待機
 * Phase 3 (Traveling): RingThicknessの幅を保ったまま外側へ拡大。MaxRadius到達で自己Destroy
 */
UCLASS()
class PRJ_TIDE_P0_API ABossShockwaveActor : public AActor
{
	GENERATED_BODY()

public:

	ABossShockwaveActor();

	// 次に生成されるショックウェーブのWaveId
	static int32 PeekNextWaveId();

	int32 GetWaveId() const { return WaveId; }

	// プロト: 攻撃エントリのダメージをAttackExecutionから流し込む
	void SetDamage(float InDamage) { Damage = InDamage; }

	// プロト: 攻撃エントリのヒットリアクションをAttackExecutionから流し込む
	void SetHitReactionTag(FGameplayTag InTag) { HitReactionTag = InTag; }

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 見た目のリング ※判定はApplyBandDamageで行うためコリジョンは持たない
	UPROPERTY(VisibleAnywhere, Category = "Tide|VFX")
	TObjectPtr<UProceduralMeshComponent> RingMesh = nullptr;

	// Phase 1: 出現待機の秒数
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float RiseDuration = 0.5f;

	// Phase 2: 拡大開始までの待機秒数
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float HoldDuration = 3.0f;

	// Phase 3: 拡大速度
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float ExpandSpeed = 800.0f;

	// 拡大しきって消滅する半径
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float MaxRadius = 1500.0f;

	// 拡大開始時のリング半径
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float InitialRadius = 100.0f;

	// リングの半径方向の幅 ※半径が増えても一定でダメージ帯の太さもこれ
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float RingThickness = 150.0f;

	// 波の高さ。Risingで0 -> WaveHeightに立ち上がり、
	// Travelingで拡大につれ減衰する
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float WaveHeight = 150.0f;

	// Traveling中に高さ減衰を始めるタイミング (InitialRadius ->
	// MaxRadius区間の何割で開始するか)0.0は旧挙動互換で、
	// 拡大開始直後から線形減衰するそれより大きい値ではWaveHeightを維持する区間ができ、
	// 終端までに0へ落とす
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightFadeStartRadiusRatio = 0.0f;

	// 円周の分割数 ※多いほど滑らかだが処理負荷が増える
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave", meta = (ClampMin = "8"))
	int32 RingSegments = 64;

	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float Damage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	FGameplayTag HitReactionTag;

	// 地形に沿わせる下方向トレースの上端高さ(リング中心Zから上)
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float GroundTraceUpDistance = 1000.0f;

	// 地形に沿わせる下方向トレースの距離
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	float GroundTraceDownDistance = 5000.0f;

	// リングのマテリアル。Opacityスカラーパラメータでフェードする
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	TObjectPtr<UMaterialInterface> RingMaterial = nullptr;

	// フェード開始タイミング (MaxRadiusの何割から透明にしはじめるか)
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeStartRadiusRatio = 0.8f;

	// マテリアル側のOpacityスカラーパラメータ名
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	FName OpacityParamName = TEXT("Opacity");

	// ジャンプ回避判定をデバッグ描画する (緑 = 回避成立 / 赤 = ヒット)
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Shockwave")
	bool bDebugDrawJumpCheck = false;

private:

	enum class EPhase : uint8 { Rising, Holding, Traveling };

	// 指定半径・高さでリングメッシュ (断面が長方形の壁状リング) を再構築する
	// 地形トレースで各セグメントの底辺を地面に合わせ、Heightぶん上へ立ち上げる
	void BuildRingMesh(float DisplayRadius, float Height);

	// 中心からの2D半径バンド内の敵にダメージ/光輪を破砕する
	void ApplyBandDamage();

	// 指定XYの地面の高さ (ワールドZ) を返す。未ヒット時はアクターZを返す
	// AlsoIgnore: トレースで無視する追加アクター (PC自身を地面と誤認しないため)
	float GroundZAt(const FVector2D& WorldXY, const AActor* AlsoIgnore = nullptr) const;

	EPhase Phase         = EPhase::Rising;
	float  RiseElapsed   = 0.0f;
	float  HoldElapsed   = 0.0f;
	float  CurrentRadius = 0.0f;
	float  CurrentHeight = 0.0f;
	int32  WaveId = 0;

	TSet<TObjectPtr<AActor>> HitActors;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial = nullptr;

};
