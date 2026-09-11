// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

/**
 * カメラデバッグImGuiクラス
 */
class InputWindow : public ImGuiWindowBase
{
public:
	const char* GetWindowName() const override { return "InputCheck"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::System; }

	void DrawContents() override;

private:
	// UIのスケール値
	float UIScale = 1.0f;
};

#endif // !UE_BUILD_SHIPPING

