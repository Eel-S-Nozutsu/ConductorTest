// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

/**
 * カメラデバッグImGuiクラス
 */
class CameraWindow : public ImGuiWindowBase
{
public:
	const char* GetWindowName() const override { return "Camera"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Gameplay; }

	void DrawContents() override;

private:
	// ImGuiのテキスト入力用バッファ
	char InputRowName[128] = "SimpleLockOn";
	char InputBlendRowName[128] = "Default";

	// デバッグUIからPushしたモードのハンドルを保持するリスト
	TArray<FCameraModeHandle> DebugPushedHandles;
};

#endif // !UE_BUILD_SHIPPING

