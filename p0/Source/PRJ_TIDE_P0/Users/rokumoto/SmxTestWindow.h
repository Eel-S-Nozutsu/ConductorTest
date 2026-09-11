// Copyright (c) 2026, S.Rokumoto EelGameStudio, Inc. All Rights Reserved.

#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * rokumoto の動作確認用ImGuiウィンドウ
 */
class SmxTestWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "SmxTest"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Tools; }

	void DrawContents() override;

};

#endif // !UE_BUILD_SHIPPING
