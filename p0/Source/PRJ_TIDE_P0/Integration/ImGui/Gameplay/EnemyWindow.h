// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * 敵
 */
class EnemyWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "Enemy"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Gameplay; }
	void DrawContents() override;
	void OnOpen() override;

};
