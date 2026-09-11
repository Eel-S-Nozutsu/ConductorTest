// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/TimedSteppingStone.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "DecayedSteppingStone.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;

/**
 * 朽ちた時限性浮石。ツタで拘束され不動。スライドパッシブ(竜巻)で囲うとツタが壊れ、
 * RiseTargetの位置まで浮上し、以降は基底ATimedSteppingStoneとして振る舞う。
 *
 * 囲いトリガはIWindAffectable(ABossMineHazard・AAntlionCoreと同経路)。竜巻は範囲内の
 * AllDynamicObjectsをスイープするため、拘束中もQueryOnly + WorldDynamicでトリガを受ける
 * (NoCollisionだと拾われない)。
 *
 * 見た目は他所と同じ浮石メッシュ(基底のStoneMesh)を使い、「ツタを壊したらあの浮石になる」と気づかせる。
 *
 * 拘束中から乗れる(Pawn=Block)。ツタが壊れてから浮上開始までのFreeDuration + FreeHoldDurationが
 * 乗り込み猶予なので、一緒に浮上させたいならFreeHoldDurationを長めに取ること。
 */
UCLASS()
class PRJ_TIDE_P0_API ADecayedSteppingStone : public ATimedSteppingStone, public IWindAffectable
{
	GENERATED_BODY()

public:

	ADecayedSteppingStone();

	// IWindAffectable：囲われた瞬間をツタ破壊トリガに使う ※1回だけラッチ
	virtual void OnWindEnter(const FWindInfluence& Wind) override;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void TickPreSettled(float DeltaTime) override;

#if WITH_EDITOR
	// エディタのビューポートでもTickさせ、RiseTargetへの線・球を毎フレーム描く
	// (配置確認しやすく)
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

public:

	// 拘束を表すツタ ※ディゾルブ用スカラーパラメータを持つマテリアルにする
	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick|Decayed")
	TObjectPtr<UStaticMeshComponent> VineMesh;

	// 浮上先。ビューポートのドラッグハンドルで置く。到達点が新しい定位置になる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick|Decayed", meta = (MakeEditWidget = true))
	TObjectPtr<USceneComponent> RiseTarget;

	// ツタのディゾルブ量を渡すスカラーパラメータ名(1=無傷/0=消滅)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed")
	FName DissolveParamName = TEXT("Dissolve_Alpha");

	// ツタ破壊(ディゾルブ)にかける秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed", meta = (ClampMin = "0.0"))
	float FreeDuration = 0.6f;

	// ツタが消えてから浮上を始めるまでの待機秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed", meta = (ClampMin = "0.0"))
	float FreeHoldDuration = 0.3f;

	// 浮上にかける秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed", meta = (ClampMin = "0.01"))
	float EmergeDuration = 3.0f;

	// 浮上中に中心軸の周りを回る角速度(度/秒)。大きいほど巻き数が増える。0で真上(螺旋なし)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed")
	float EmergeSpinSpeedDeg = 360.0f;

	// 螺旋の最大半径。始点/終点では0、中間で最大に膨らむ(地雷の巻き上げ感)。0で真上
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed", meta = (ClampMin = "0.0"))
	float EmergeSpiralRadius = 120.0f;

	// ツタ破壊時に浮石位置へ再生するエフェクト(任意)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Decayed")
	TObjectPtr<UNiagaraSystem> BreakVFX;

private:

	void EnterFreeing();
	void TickFreeing(float DeltaTime);
	void TickEmerging(float DeltaTime);

	// 二重発火ガード(複数竜巻に同時に囲まれても1回だけ)
	bool bFreed = false;

	// ディゾルブ完了時のVFX/ツタ非表示を1回だけ行うためのラッチ
	bool bVineBroken = false;

	float FreeElapsed = 0.0f;
	float EmergeElapsed = 0.0f;

	// 螺旋の現在角度(度)。Emerging中にEmergeSpinSpeedDegで進める
	float OrbitAngleDeg = 0.0f;

	// 浮上の始点・終点はワールド座標で固定する
	// RiseTargetはStoneMeshの子なので石が動くと一緒に動く
	// 石を動かす前(Emerging開始時)に一度だけ確定し、以後は再参照しない
	// (終点が石を追いかけないように)
	FVector EmergeStartLocation = FVector::ZeroVector;
	FVector EmergeTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> VineMIDs;

};
