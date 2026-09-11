// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraBlendTypes.h"

#include "ExCameraSubsystem.generated.h"

class UDataTable;
class UExCameraModeComponent;
class UExCameraMode;
class UExCameraModeParam;

UCLASS()
class UExCameraSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// CameraModeComponent は CameraActor の生存期間に依存するため、
	// LocalPlayerSubsystem 側には弱参照で登録して扱う
	void RegisterCameraModeComponent( UExCameraModeComponent* InComponent );
	void UnregisterCameraModeComponent( UExCameraModeComponent* InComponent );

	UFUNCTION( BlueprintCallable, Category = "Camera System" )
	FCameraModeHandle PushCameraMode( UExCameraModeParam* Param, FName BlendRowName = NAME_None );

	// 三人称カメラだけはテーブル行キーで指定できる専用 API を用意
	// （ただし内部では解決済みパラメータへ変換し、共通の Push ルートへ流している）
	UFUNCTION( BlueprintCallable, Category = "Camera System" )
	FCameraModeHandle PushThirdPersonCameraByKey( FName RowName, FName BlendRowName = NAME_None );

	// bForceFixedBlendStart=true で、退場モードを毎フレーム再計算せず開始時のスナップショットで固定する。
	// ロックオン解除のように「退場側が追従対象を失って構図を急に作り直す」のを防ぎ、開始構図から滑らかに戻したい時に使う。
	UFUNCTION( BlueprintCallable, Category = "Camera System" )
	bool PopCameraMode( FCameraModeHandle Handle, FName BlendRowName = NAME_None, bool bForceFixedBlendStart = false );

	void SetThirdPersonCameraTable( UDataTable* InTable );
	void SetThirdPersonModeClass( TSubclassOf<UExCameraMode> InModeClass );
	void SetBlendSettingsTable( UDataTable* InTable );

	// 現在アクティブなカメラモードの回転速度倍率を取得する
	FVector2D GetActiveCameraRotationSpeedRate() const;

	// デバッグ用
	UExCameraModeComponent* GetCameraModeComponent() const;

private:
	// ブレンド設定をテーブルから取得する内部関数
	FExCameraBlendTableRow GetBlendSettings( FName BlendRowName ) const;

private:
	// 三人称カメラで使用する CameraMode
	UPROPERTY()
	TSubclassOf<UExCameraMode> ThirdPersonModeClass;

	// 三人称カメラ専用のパラメータテーブル
	UPROPERTY()
	TObjectPtr<UDataTable> ThirdPersonCameraTable;

	// ブレンドの遷移ルールをまとめたテーブル
	UPROPERTY()
	TObjectPtr<UDataTable> BlendSettingsTable;

private:
	// LocalPlayerSubsystem は CameraActor / Component より長生きするため、
	// CameraMpdeComponent は必ず弱参照で保持し、呼び出し時に有効性を確認すること
	TWeakObjectPtr<UExCameraModeComponent> RegisteredCameraModeComponent;
};
