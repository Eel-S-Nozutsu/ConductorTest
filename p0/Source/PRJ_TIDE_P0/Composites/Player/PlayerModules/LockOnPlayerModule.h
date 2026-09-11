// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "LockOnPlayerModule.generated.h"

class ULockOnTargetComponent;

UCLASS()
class ULockOnPlayerModule : public  UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( class ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void ToggleLockOn();
	bool IsLockOnActive() const;

	// ロックオン中の特殊な移動処理（カニ歩き）を行う。処理した場合は true を返す
	bool TryProcessMovementInput( float Right, float Forward, const FVector2D& ClampedInput );

	// 右スティックによるターゲット切り替え要求
	// @param InputValue スティックの入力値（正 : 右, 負 : 左）
	void RequestTargetSwitch( float InputValue );

#if !UE_BUILD_SHIPPING
public:
	// ImGuiのウィンドウ（またはパネル）にモジュールとコンポーネントの状態を描画する
	void DrawDebugImGui();
#endif

private:
	void UpdateLockOnCondition( float DeltaTime );
	void ClearTarget();

	void OnStartLockOn();
	void OnEndLockOn();

	// 指定された方向（カメラ基準）で最適な次ターゲットを探す
	ULockOnTargetComponent* FindSwitchCandidate( bool bIsRight ) const;

	void DrawLockOnMarkerDebugUI( float DeltaTime );

private:
	float LockOnMarkerRotation = 0.0f;
	FAutomaticTimer CurrentLostTimer;

	FCameraModeHandle LockOnCameraHandle;
};
