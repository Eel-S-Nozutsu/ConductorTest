// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplineWavingFloorGimmick.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * スプラインに沿って並ぶ連続メッシュが、指定した高さ・頻度・速度で波打つ床ギミック(BP_SplineWavingFloor_v2のコード版)。
 *
 * 使い方:
 *  1. PathSplineの始点/終点をビューポートでドラッグして全体の長さ・経路(カーブ可)を決める
 *  2. NumDivisionsに欲しい区間数(床の枚数)を入力する。スプラインの点数はNumDivisions+1個になる
 *  3. FloorMeshに区間1つ分のStaticMeshを設定する(ローカルX+方向を長さ方向として伸縮配置される)
 * 上記のいずれを編集してもRegenerateFloor()が走り、スプライン全長をNumDivisions個に均等分割した
 * 点群を(現在のカーブ形状を弧長サンプリングして)再生成し、区間ごとにSplineMeshComponentを張り直す。
 * ビューポートでのスプライン点ドラッグはOnConstruction経由、Detailsパネルでのプロパティ変更は
 * PostEditChangeProperty経由でそれぞれ拾って自動再生成する。
 *
 * 再生中はTickで各分割点をサイン波で上下させ、区間メッシュがその形に追従して波打つ。
 * 波の式は物理準拠(速度 = 頻度 × 波長)を採用し、WaveSpeed/WaveFrequencyから波長を逆算している。
 */
UCLASS()
class PRJ_TIDE_P0_API ASplineWavingFloorGimmick : public AActor
{
	GENERATED_BODY()

public:

	ASplineWavingFloorGimmick();

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// スプライン全長をNumDivisions個の区間に均等分割し、区間ごとのSplineMeshComponentを張り直す。
	// エディタでの編集に対しては自動で呼ばれるが、手動で強制再生成したい場合にも押せるボタンとして公開する
	UFUNCTION(CallInEditor, Category = "Tide|SplineWavingFloor")
	void RegenerateFloor();

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|SplineWavingFloor")
	TObjectPtr<USceneComponent> SceneRoot;

	// 経路兼床の基準スプライン。始点/終点をドラッグして全長・カーブを決める
	UPROPERTY(VisibleAnywhere, Category = "Tide|SplineWavingFloor")
	TObjectPtr<USplineComponent> PathSpline;

	// 床の区間数。この数だけ均等にスプラインポイントが再配置される(点数はNumDivisions+1)
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor", meta = (ClampMin = "1", UIMin = "1"))
	int32 NumDivisions = 8;

	// 区間1つ分の床メッシュ。ローカルX+方向を長さ方向として伸縮配置される
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor|Mesh")
	TObjectPtr<UStaticMesh> FloorMesh;

	// FloorMeshに上書きするマテリアル(未設定ならメッシュ本来のマテリアルを使用)
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor|Mesh")
	TObjectPtr<UMaterialInterface> FloorMaterialOverride;

	// 波の高さ(振幅) [uu]
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor|Wave", meta = (ClampMin = "0.0"))
	float WaveHeight = 30.0f;

	// 波の頻度 [Hz] (1秒あたりの振動回数)
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor|Wave", meta = (ClampMin = "0.0"))
	float WaveFrequency = 0.5f;

	// 波がスプラインに沿って流れる速さ [uu/sec] (速度 = 頻度 × 波長 の関係で波長を逆算する)
	UPROPERTY(EditAnywhere, Category = "Tide|SplineWavingFloor|Wave")
	float WaveSpeed = 200.0f;

private:

	// RegenerateFloor()時点のスプライン各点(波の基準となる静止位置)。ローカル座標
	TArray<FVector> BaseLocalPositions;

	// RegenerateFloor()時点の各点の、始点からの弧長距離
	TArray<float> BaseDistances;

	// 波アニメーション用の経過時間
	float ElapsedTime = 0.0f;

	// 生成済みの区間メッシュ(RegenerateFloor()で張り直す)
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> FloorSegments;

	// 生成済み区間メッシュを全て破棄する
	void ClearFloorSegments();

	// PathSplineの現在の点群からBaseLocalPositions/BaseDistances(波の基準となる静止形状)を再計算する。
	// BaseLocalPositions/BaseDistancesはUPROPERTYではないため、PIE用のアクター複製では値が引き継がれない。
	// そのためBeginPlayで必ず呼び直し、複製直後でも波の基準形状を確実に持たせている
	void CaptureBaseShape();

	// PathSplineの点SegmentIndex/SegmentIndex+1の現在位置・接線から区間メッシュの形を更新する
	void UpdateSegmentFromSpline(int32 SegmentIndex);
};
