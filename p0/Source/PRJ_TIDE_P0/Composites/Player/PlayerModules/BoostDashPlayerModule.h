// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "BoostDashPlayerModule.generated.h"

class UExCameraSubsystem;

// 加速ギミックから駆動されるブーストダッシュ。現在の向きへ MaxSpeed ぶん加算し（上書きはしない）、
// Duration の間その速度を上限に走る（方向転換は可能）。速度・時間はギミック側の値をそのまま使う。
//
// モーション・終了挙動はチャージダッシュに準拠：入力の有無に関わらず ST→LP を再生し、時間切れで移動入力が
// あれば通常ダッシュへ、無ければ ChargeDashEnd でブレーキ。空中では一定時間だけ重力をカットして水平を維持する。
//
// 触れた瞬間のチャージ状態の扱い（どの状況でも専用モーションで統一する）：
//  ・溜め中 → 中断してギア／コンボ段を退避し、終了時に長押し継続なら退避ギアのまま再開する
//    （bBoostKeepChargeMotionWhileCharging で旧仕様＝溜めを維持し速度加算のみへ戻せる）
//  ・チャージダッシュ中 → 残り持続時間ごと退避し、終了時に触れた瞬間のギアのまま続きから再生する
//  ・チャージジャンプ中 → 中断のみ（再開しない）。中断しないと IsPlayingChargeAction() が true のまま残り、
//    専用モーションが CHARGE_JUMP_LP に上書きされる
UCLASS()
class UBoostDashPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( class ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// 加速ギミックから呼ぶ。VelocityCap＝加算後の水平速度上限、
	// AirZUpSpeed＝空中取得時のみ加算する上方向速度（どちらも 0 以下で無効）
	void BeginBoost( float MaxSpeed, float Duration, float VelocityCap = 0.0f, float AirZUpSpeed = 0.0f, float StartMontagePlayRate = 1.0f );

	bool  IsBoosting() const { return bIsBoosting; }
	float GetBoostMaxSpeed() const { return BoostMaxSpeed; }	// MaxWalkSpeed の上書きに使う

	// 地上ブースト終了直後の滑らか旋回中か（この間は Dash 側の Turn 検知を抑止する）
	bool  IsGroundExiting() const { return !BoostGroundExitTimer.IsFinish(); }

private:
	void UpdateBoostMontageState( float DeltaTime );	// ST 再生中は割り込まず、終了後にループへ繋ぐ
	void UpdateGravityLock( float DeltaTime );		// GravityLockTimer が有効な間だけ Z 速度と重力を殺す

	// 旋回力は地上/空中で別値のため、接地状態が変わったら移動パラメータを張り直す。RefreshMovementParams は
	// 状態遷移時にしか呼ばれず崖落ち・着地での切替に追従できないため、自前で検知する
	void UpdateBoostTurnParams();

	// 空中は AirControl 制限の加速度では高速のブースト速度を曲げられずメッシュだけ回ってしまうため、
	// velocity を能動的に旋回させる
	void UpdateBoostAirSteering();

	void EndBoost();	// 移動入力があれば通常ダッシュへ、無ければ ChargeDashEnd を再生して戻す

	// 終了瞬間に入力方向へ即スナップしてカクッと曲がるのを防ぐ（UpdateBoostAirSteering の地上・時限版）
	void UpdateBoostGroundExit( float DeltaTime );

	// --- 残像。ChargeEffectSubModule と同方式（素材はブースト専用スロット固定）---
	void UpdateGhostTrails( float DeltaTime );
	void SpawnGhostTrail();
	class UMaterialInterface* GetBoostGhostTrailMaterial() const;

	// --- フレネル。ChargeEffectSubModule と同方式（色は SlidePassive のトレイル色固定・点滅なし）---
	void UpdateFresnelEffect( float DeltaTime );

	// --- カメラ（BoostDashCameraModeKey で指定）。キー未設定なら何もしない ---
	void PushBoostDashCamera();
	void PopBoostDashCamera();
	UExCameraSubsystem* GetCameraSubsystem() const;

	float GetInputThreshold() const;
	bool  HasMovementInput() const;

private:
	bool  bIsBoosting = false;
	float BoostMaxSpeed = 0.0f;				// ギミック指定の上限速度
	FAutomaticTimer BoostTimer;
	FAutomaticTimer GravityLockTimer;		// 空中水平維持
	FAutomaticTimer BoostGroundExitTimer;	// 終了直後の滑らか旋回

	// UpdateGravityLock が毎フレーム Velocity.Z へ書き戻す値。既定 0＝垂直を完全に止めて水平維持し、
	// AirZUpSpeed 指定時はその値を維持したまま水平を保つ
	float GravityLockZVelocity = 0.0f;

	bool  bWasFallingDuringBoost = false;	// 切り替わったら旋回パラメータを張り直す

	// UpdateBoostAirSteering の EaseOut の基準にする、減速開始時点の水平速度。-1 で未捕捉（維持区間）
	float AirDecayBaseSpeed = -1.0f;

	// --- 残像 ---
	struct FBoostGhostTrailData
	{
		TObjectPtr<class UPoseableMeshComponent> MeshComponent;
		TArray<TObjectPtr<class UMaterialInstanceDynamic>> MIDs;
		float ElapsedTime = 0.0f;
		float Lifespan = 1.0f;
	};
	TArray<FBoostGhostTrailData> ActiveGhostTrails;
	FAutomaticTimer GhostTrailSpawnTimer;

	float CurrentFresnelIntensity = 0.0f;
	FCameraModeHandle BoostDashCameraHandle;	// Push 中のカメラモード（未 Push なら無効）
};
