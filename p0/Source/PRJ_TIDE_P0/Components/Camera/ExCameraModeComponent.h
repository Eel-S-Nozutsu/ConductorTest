// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraBlendTypes.h"

#include "ExCameraModeComponent.generated.h"

class UExCameraMode;
class ATidePlayerController;

// カメラモードを管理するコンポーネント
UCLASS( ClassGroup = ( CameraSystem ), meta = ( BlueprintSpawnableComponent ) )
class UExCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExCameraModeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	FCameraModeHandle PushCameraMode( UExCameraMode* ModeInstance, const FExCameraBlendTableRow& BlendSettings );
	bool PopCameraMode( FCameraModeHandle Handle, const FExCameraBlendTableRow& BlendSettings );

private:
	// スタック変更後、アクティブモードが切り替わっていればブレンドを開始する。
	// OldActive == NewActive のときは何もしない（低優先モードの出し入れ等はブレンドを起こさない）。
	void BeginActiveTransition( UExCameraMode* OldActive, UExCameraMode* NewActive, const FExCameraBlendTableRow& BlendSettings );

public:

	// 入力データ供給元をセット（nullptrで解除も可）
	void SetInputProvider( ATidePlayerController* InProvider );

	FVector2D GetActiveRotationSpeedRate() const;

	// ジャンプ注視点の縦“置いていき”状態の共有ストア。カメラモードを跨いで継続させるためモードではなくここで保持し、
	// 各 UThirdPersonExCameraMode が起動時に種として読み、毎フレーム書き戻す
	FJumpFocusVerticalState& GetJumpFocusVerticalState() { return JumpFocusVerticalState; }
	const FJumpFocusVerticalState& GetJumpFocusVerticalState() const { return JumpFocusVerticalState; }

	// --- デバッグ用：現在のカメラモードのスタックを取得 ---
	const TArray<TObjectPtr<UExCameraMode>>& GetModeStack() const { return ModeStack; }
	// 現在アクティブ（最高優先度・同値は最後尾）なモードのスタック内 index。空なら INDEX_NONE
	int32 GetActiveModeIndex() const;
	// 現在アクティブなモードを取得（無ければ nullptr）
	UExCameraMode* GetActiveMode() const;
	// 前フレームで計算された最終的なカメラ座標・回転・FOVを取得
	const FMinimalViewInfo& GetLastFrameViewInfo() const { return LastFrameViewInfo; }
	// 現在モード間のブレンド（補間）が実行中かどうかを取得
	bool IsBlending() const { return bIsBlending; }
	// 現在のブレンド処理の経過時間を取得
	float GetBlendTimeElapsed() const { return BlendTimeElapsed; }
	// 現在のブレンド処理の総目標時間を取得
	float GetCurrentBlendTime() const { return CurrentBlendSettings.BlendTime; }

protected:
	// カメラ状態管理構造
	UPROPERTY( Transient )
	TArray<TObjectPtr<UExCameraMode>> ModeStack;

private:
	// ハンドル発行用の連番カウンター
	int32 NextHandleID = 1;

	// 入力の供給元は PlayerController だが、所有はしないため弱参照で保持する
	TWeakObjectPtr<ATidePlayerController> InputProvider;

	// 前回アクティブだったモード（切り替わり検知用）
	UPROPERTY()
	TObjectPtr<class UExCameraMode> LastActiveMode;

	// 前フレームの最終的なカメラ状態（ブレンドのスタート地点として使う）
	FMinimalViewInfo LastFrameViewInfo;
	// ブレンド開始時のカメラ状態（Lerpの始点として固定する）
	FMinimalViewInfo BlendStartViewInfo;
	// ブレンド開始時点の、退場モードの「理想状態」（差分計算の基準点）
	FMinimalViewInfo BlendStartOutgoingIdealView;

	// 現在適用中のブレンド設定
	FExCameraBlendTableRow CurrentBlendSettings;

	// ブレンド状態の管理
	bool bIsBlending = false;
	float BlendTimeElapsed = 0.0f;

	UPROPERTY()
	TObjectPtr<UExCameraMode> BlendingOutMode;

	// ジャンプ注視点の縦“置いていき”状態（カメラモードを跨いで継続させるための共有ストア）
	FJumpFocusVerticalState JumpFocusVerticalState;
};
