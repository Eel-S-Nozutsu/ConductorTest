// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ImGui/ImGuiWindowBase.h"
#include "ImGuiDebugSubsystem.generated.h"

/**
 * ゲーム側でのImGui管理サブシステム
 */
UCLASS()
class CONDUCTORTEST_API UImGuiDebugSubsystem
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
	virtual bool IsTickable() const override { return !UE_BUILD_SHIPPING; }

	void ToggleImGui() { bShowImGui = !bShowImGui; }

private:

#if WITH_EDITOR
	void OnEndPIE(const bool bIsSimulating);
#endif
	void RegisterWindow(TSharedPtr<ImGuiWindowBase> Window);
	void SaveImGuiState();
	void LoadImGuiState();

	TArray<TSharedPtr<ImGuiWindowBase>> Windows;

	bool bShowDemoWindow = false;
	bool bShowImGui = true;

	int32 SelectedButton = -1;

};
