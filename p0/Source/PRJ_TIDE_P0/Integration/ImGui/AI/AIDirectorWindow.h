// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * AIDirector (AttackToken・スロット占有)
 */
class AIDirectorWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "AIDirector"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::AI; }
	void DrawContents() override;

};
