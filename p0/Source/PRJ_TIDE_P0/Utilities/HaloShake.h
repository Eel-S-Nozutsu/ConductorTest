// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HaloShake.generated.h"

/**
 * 光輪が「ヒットしたが壊れなかった」ときの3軸回転ブレ設定。
 * HaloComponent (ガード・背面待機) / FPartHaloConfig (部位) で共有する。
 */
USTRUCT(BlueprintType)
struct FHaloHitShakeParams
{
	GENERATED_BODY()

	// 揺れる秒数 ※0以下でブレ無効
	UPROPERTY(EditAnywhere, Category = "HaloShake")
	float Duration = 0.25f;

	// 各軸の最大振れ角
	UPROPERTY(EditAnywhere, Category = "HaloShake")
	float AmplitudeDeg = 8.0f;

	// 揺れの周波数
	UPROPERTY(EditAnywhere, Category = "HaloShake")
	float FrequencyHz = 28.0f;

};

/**
 * 光輪メッシュを基準姿勢の周りで3軸回転ブレさせる実行時状態。
 * UPROPERTY不要の通常のstruct。所有者コンポーネントのTickからTick()を毎フレーム呼ぶ。
 */
struct FHaloShakeState
{
	// 揺れを開始/再トリガーする。Meshの現在の相対回転を基準として記録する
	// 既に揺れ中なら基準姿勢は維持したまま振り直す (オフセット込みの回転を基準にしない)
	void Trigger(USceneComponent* Mesh, const FHaloHitShakeParams& Params);

	// 1フレーム分進める。完了したら基準姿勢へ戻して停止する
	void Tick(float DeltaTime, USceneComponent* Mesh);

	bool IsActive() const { return bActive; }

private:

	bool     bActive      = false;
	float    Elapsed      = 0.0f;
	float    Duration     = 0.0f;
	float    AmplitudeDeg = 0.0f;
	float    FrequencyHz  = 0.0f;
	FRotator BaseRelRot   = FRotator::ZeroRotator;

};
