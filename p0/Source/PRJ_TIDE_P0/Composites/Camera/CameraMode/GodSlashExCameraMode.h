// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"

#include "GodSlashExCameraMode.generated.h"

// 神技・一閃の演出専用カメラモード。GodActionPlayerModule が発動時に Push し終了時に Pop する。
// フレーミング情報は GetGodSlashCameraFraming() 経由で受け取り、プレイヤーと敵が収まる 2 ショット（ED では正面ショット）を
// 構成する。数値は PlayerParamData（GodSlashCam*）で調整する
UCLASS( Blueprintable )
class UGodSlashExCameraMode : public UExCameraMode
{
	GENERATED_BODY()

public:
	virtual void UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo ) override;
	virtual void OnActivated( const FMinimalViewInfo& LastViewInfo ) override;

	// 演出カメラの基本 FOV
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|GodSlash Camera" )
	float FOV = 75.0f;

private:
	// 直前フレームの計算結果（カット切り替えをスムーズにするための補間元）
	bool bInitialized = false;
	FVector CurrentLocation = FVector::ZeroVector;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	float CurrentFOV = 75.0f;

	// カット中のポーズ固定：同じ CutIndex の間はカメラのポーズを固定し、プレイヤーがワープ移動しても動かさない
	// （固定ショットの中を斬り抜ける絵にする）。構図の計算し直しは CutIndex が変わったときだけ
	int32 FramingCutIndex = -1;					// 現在固定中のカット番号（-1 で未選択）
	FVector FramedLoc = FVector::ZeroVector;	// 固定中のカメラ位置
	FRotator FramedRot = FRotator::ZeroRotator;	// 固定中のカメラ回転
	float FramedFOV = 75.0f;					// 固定中の FOV

	// ワイドカット演出用：発動（Push）時点の TPS カメラ姿勢を基準として保持する。
	// 「元の TPS カメラ位置から少し引いて少し上げる」の起点として使い、演出中は変化しない。
	FVector WideCutBaseLoc = FVector::ZeroVector;
	FRotator WideCutBaseRot = FRotator::ZeroRotator;
	float WideCutBaseFOV = 75.0f;
};
