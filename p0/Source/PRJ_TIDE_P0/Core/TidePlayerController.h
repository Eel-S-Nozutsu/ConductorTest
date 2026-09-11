// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraBlendTypes.h"

#include "TidePlayerController.generated.h"

class UInputRouterComponent;
class UDPadFromHatComponent;
class UInputMappingContext;
class UInputAction;
class AExCameraActor;
class UExCameraMode;

// PRJ_TIDE 用の PlayerController
UCLASS( abstract )
class ATidePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATidePlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void SetupInputComponent() override;
	virtual void Tick( float DeltaTime ) override;

	// 入力開始時
	UFUNCTION() void HandleActionStarted( const FName& ActionName, const FInputActionValue& Value );
	// 入力開始/押下中
	UFUNCTION() void HandleActionTriggered( const FName& ActionName, const FInputActionValue& Value );
	// 入力解除時
	UFUNCTION() void HandleActionCompleted( const FName& ActionName, const FInputActionValue& Value );

	// コンポーネントから呼ばれ、現在の入力とプレイヤー状態を渡す
	virtual FCameraControlData	GetCameraControlData() const;

	// D-Pad(ハット軸)変換コンポーネントを取得する（神鳥検証モードの神技メニュー左右選択のポーリング用）
	UDPadFromHatComponent* GetDPadFromHat() const { return DPadFromHat; }

	// カットシーン中かどうかを返す (APlayerController::bCinematicModeを公開するだけ)
	bool IsInCinematicMode() const { return bCinematicMode; }

private:
	bool ProcessConsoleExec( const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor ) override;

	void InitializeCamera();

	void InitializeInput();
	void InitializeInputHandlers();

	void ToggleImGui();
	void UpdateInputModeByImGui();

	// R1+Select 同時押しでデバッグカメラを起動する（毎フレーム検知。終了側はデバッグカメラ側で処理）
	void UpdateDebugCameraEnterInput();

	// 神技3択・カーソルモードの選択入力を毎フレーム検知する（自由移動時は L1/R1、移動禁止時は L スティック水平）。
	// 物理キーを直接ポーリングするのは L1/R1 が IA_Dodge と共用でハンドラから左右を判別できないため
	void UpdateGodArtSelectInput();

protected:
	// todo:↓これらのプロパティは将来的にエンジンのConfigに移すことも検討 ---

	// 生成するカメラクラスの指定
	UPROPERTY( EditDefaultsOnly, Category = "Tide|Camera System" )
	TSubclassOf<AExCameraActor> CameraActorClass;

	// ゲーム開始時にデフォルトでPushするCameraModeのテーブル行指定
	UPROPERTY( EditDefaultsOnly, Category = "Tide|Camera System" )
	FName DefaultCameraModeRowName = FName( "Default" );

	// 三人称カメラ用のパラメータテーブル
	UPROPERTY( EditDefaultsOnly, Category = "Tide|Camera System" )
	TObjectPtr<UDataTable> ThirdPersonCameraTable;

	// 三人称カメラ起動時に使うCameraModeクラス
	UPROPERTY( EditDefaultsOnly, Category = "Tide|Camera System" )
	TSubclassOf<UExCameraMode> ThirdPersonCameraModeClass;

	// ブレンド設定をまとめたDataTable
	UPROPERTY( EditDefaultsOnly, Category = "Tide|Camera System" )
	TObjectPtr<class UDataTable> BlendSettingsTable;

	// todo:↑これらのプロパティは将来的にエンジンのConfigに移すことも検討 ---

private:
	UPROPERTY( Transient )
	TObjectPtr<AExCameraActor> SpawnedCamera;

	// 現在適用中のブレンド設定を保持する構造体
	FExCameraBlendTableRow CurrentBlendSettings;

	// 入力中継コンポーネント。MappingContextList（有効化する IMC 群）をここで設定するため、
	// BP のコンポーネント/詳細パネルで見える・編集できるよう VisibleAnywhere にする（DPadFromHat と同様）。
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "Tide|Input", meta = ( AllowPrivateAccess = "true" ) )
	TObjectPtr<UInputRouterComponent> InputRouter;

	// D-Pad(ハット軸)→ Gamepad_DPad_* 変換コンポーネント
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "Tide|Input", meta = ( AllowPrivateAccess = "true" ) )
	TObjectPtr<UDPadFromHatComponent> DPadFromHat;

	struct FInputActionHandlers
	{
		TFunction<void( const FInputActionValue& )> OnStarted;
		TFunction<void( const FInputActionValue& )> OnTriggered;
		TFunction<void( const FInputActionValue& )> OnCompleted;
	};
	TMap<FName, FInputActionHandlers> InputHandlers;

	bool bLastImGuiInputState = false;

	// デバッグカメラ起動チョード（R1+Select）の前フレーム状態（エッジ検出用）
	bool bPrevDebugCamEnterChord = false;

	// 神技選択の L1/R1 立ち上がり検出用の前フレーム押下状態
	bool bPrevGodArtSelectL1 = false;
	bool bPrevGodArtSelectR1 = false;
};
