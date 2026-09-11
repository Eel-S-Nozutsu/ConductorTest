// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"

#include "AirChargeDashExCameraMode.generated.h"

// 空中チャージダッシュ専用カメラモード（斜め下ダイブ用）。Push されるとダイブ方向の後ろへ「グイっと」回り込む。
// 寄せ演出（SwingDuration）中のみカメラ入力をロックし、完了で ControlRotation をダイブ角へ同期して解除する。
// 以降は bHoldUntilDashEnd なら専用カメラを保持したままスティックで軌道を回せる（Pop はダッシュ終了時）。
//
// ダイブ方向は GetAirChargeDashCameraFraming() 経由で受け取り、カメラ位置の軌道方向に使う。カメラの向きは
// 毎フレーム注視点を見るように作り直すため、加速落下で位置追従が遅れてもプレイヤーが画面下に見切れない
UCLASS( Blueprintable )
class UAirChargeDashExCameraMode : public UExCameraMode
{
	GENERATED_BODY()

public:
	// 基底は Priority のみ反映するため、FOV も本モードの FOV へ取り込む
	virtual void InitializeMode( const FExCameraModeCommonParams& Params ) override;

	virtual void UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo ) override;
	virtual void OnActivated( const FMinimalViewInfo& LastViewInfo ) override;

	// 入力ロックの実体は ATidePlayerCharacter::RequestLook 側（IsAirChargeDashCameraSwinging で寄せ演出中
	// のみ早期 return）。こちらは現状未配線の保険
	virtual FVector2D GetRotationSpeedRate() const override { return FVector2D( 1.0f, 1.0f ); }

	// --- カメラ調整数値（BP で調整する）---

	// 専用カメラの基本 FOV。DA の CommonParams.FOV が設定されていれば InitializeMode でそれに置き換わる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera" )
	float FOV = 60.0f;

	// 背後カメラの引き距離（cm）。ダイブ方向の逆側にこの距離だけ離してカメラを置く
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera" )
	float ArmLength = 700.0f;

	// 注視基準（プレイヤー位置）からの高さオフセット（cm）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera" )
	float HeightOffset = 0.0f;

	// 見下ろし角（度・負で見下ろす）。ダッシュ進行方向は水平なので「斜め下」はカメラの Pitch をこの明示角で作る
	// ※目標回転の計算に使うため、モジュール側が CDO からこの値を読む
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera", meta = ( ClampMin = "-89.0", ClampMax = "89.0" ) )
	float DivePitchDegrees = -35.0f;

	// 見下ろし角の適用率（0=水平キープ／1=DivePitchDegrees を完全適用）。実際の Pitch は DivePitchDegrees × この値。
	// ※目標回転の計算に使うため、モジュール（UChargeActionPlayerModule_V2）が CDO からこの値を読む
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float PitchAlignRate = 1.0f;

	// 位置・回転の内部補間スピード（大きいほど素早く目標構図へ寄る）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera", meta = ( ClampMin = "0.0" ) )
	float InterpSpeed = 30.0f;

	// 「グイっと」寄せてから通常カメラへ操作を返すまでの時間（秒）。この間はカメラ入力をロックする。
	// ※寄せ演出のライフサイクルはモジュールが持つため、モジュールが CDO からこの値を読む
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera", meta = ( ClampMin = "0.0" ) )
	float SwingDuration = 0.35f;

	// 寄せ演出の発動許容角（度・Yaw）。発動時点の実カメラ Yaw が目標構図とこの角度以内なら寄せ演出を行い、超えていれば
	// 専用カメラを出さず通常カメラのままにする（大振りなスイングを避ける）。180 で常に発動 ※モジュールが CDO から読む
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera", meta = ( ClampMin = "0.0", ClampMax = "180.0" ) )
	float SwingStartMaxYawDiffDegrees = 30.0f;

	// ON で SwingDuration 経過後も Pop せず**着地するまで**専用カメラを保持する（Pop は OnLanded。空中でのキャンセル・
	// 死亡時は ResetChargedActionState が保険で Pop する）。通常 TPS は縦デッドゾーンで高速落下を置いていくため
	// ダイブ途中で返すと画面下に見切れるが、本カメラは常に注視点を見るので保持すれば見切れない ※モジュールが CDO から読む
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|AirChargeDash Camera" )
	bool bHoldUntilDashEnd = true;

private:
	// 直前フレームの計算結果。Push 直後のブレンドを滑らかにするための補間元
	bool bInitialized = false;
	FVector CurrentLocation = FVector::ZeroVector;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	float CurrentFOV = 60.0f;
};
