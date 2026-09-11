// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DebugCameraController.h"

#include "TideDebugCameraController.generated.h"

/**
 * UE標準デバッグカメラ（ADebugCameraController）の拡張。
 * ・R1（右ショルダー）長押しで高速移動 / L1（左ショルダー）長押しで低速移動
 * ・R1 + Select 同時押しでデバッグカメラを終了
 * ・X（□）ボタンで元プレイヤーをデバッグカメラ視点へテレポート
 *
 * ADebugCameraController は Tick を持たず（スポーン直後は AActor::Tick が回らない）ため、
 * 入力はエンジン同様 SetupInputComponent の BindKey（イベント駆動）で受ける。
 * UTideCheatManager が DebugCameraControllerClass に本クラスを指定して使用する。
 */
UCLASS()
class ATideDebugCameraController : public ADebugCameraController
{
	GENERATED_BODY()

public:
	ATideDebugCameraController();

protected:
	virtual void SetupInputComponent() override;

	// 通常時（R1/L1 とも押していない）の移動速度スケール
	UPROPERTY( EditAnywhere, Category = "Tide|DebugCamera" )
	float NormalSpeedScale = 1.0f;

	// R1 長押し中の移動速度スケール（高速）
	UPROPERTY( EditAnywhere, Category = "Tide|DebugCamera" )
	float FastSpeedScale = 4.0f;

	// L1 長押し中の移動速度スケール（低速）
	UPROPERTY( EditAnywhere, Category = "Tide|DebugCamera" )
	float SlowSpeedScale = 0.25f;

	// テレポート時にデバッグカメラ前方へ飛ばすトレース距離
	UPROPERTY( EditAnywhere, Category = "Tide|DebugCamera" )
	float TeleportTraceDistance = 100000.0f;

private:
	// 入力ハンドラ
	void OnR1Pressed();
	void OnR1Released();
	void OnL1Pressed();
	void OnL1Released();
	void OnSelectPressed();
	void OnTeleportPressed();

	// R1/L1 の押下状態に応じて移動速度スケールを反映する
	void ApplyHoldSpeedScale();
	// デバッグカメラを終了する（元コントローラの CheatManager 経由）
	void ExitDebugCameraNow();
	// 元プレイヤーをデバッグカメラ視点の先へテレポートする
	void TeleportOriginalPawn();

	bool bR1Held = false;
	bool bL1Held = false;
};
