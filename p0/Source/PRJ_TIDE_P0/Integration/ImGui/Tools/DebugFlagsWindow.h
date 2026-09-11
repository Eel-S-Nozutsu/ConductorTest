// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * デバッグフラグを一括管理するウィンドウ
 */
class DebugFlagsWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "DebugFlags"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Tools; }

	void DrawContents() override;

};

#endif // !UE_BUILD_SHIPPING
