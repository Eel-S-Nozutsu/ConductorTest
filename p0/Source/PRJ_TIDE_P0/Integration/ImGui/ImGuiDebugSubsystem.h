// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiInspector.h"
#include "ImGuiDebugSubsystem.generated.h"

/**
 * ゲーム側でのImGui管理サブシステム
 */
UCLASS()
class PRJ_TIDE_P0_API UImGuiDebugSubsystem
	: public UGameInstanceSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:

	// UGameInstanceSubsystem
	virtual void Initialize( FSubsystemCollectionBase& Collection ) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;
	// ImGui モジュールは Shipping でもリンクされるため UE_WITH_IMGUI では落ちない。Tick 自体を止めて描画を封じる
	virtual bool IsTickable() const override { return !UE_BUILD_SHIPPING; }

	void ToggleImGui() { bShowImGui = !bShowImGui; }

private:

#if WITH_EDITOR
	// PIE終了時に呼ばれる
	void OnEndPIE(const bool bIsSimulating);
#endif
	// 描画ウィンドウの登録
	void RegisterWindow(TSharedPtr<ImGuiWindowBase> Window);
	// ImGuiのウィンドウ状態を保存
	void SaveImGuiState();
	// ImGuiのウィンドウ状態を読み込み
	void LoadImGuiState();

	// 描画するウィンドウリスト
	TArray<TSharedPtr<ImGuiWindowBase>> Windows;

	// インスペクタシステム
	FImGuiInspector Inspector;

	// ImGuiDemoWindow制御
	bool bShowDemoWindow = false;
	bool bShowImGui = true;

	int32 SelectedButton = -1;
};
