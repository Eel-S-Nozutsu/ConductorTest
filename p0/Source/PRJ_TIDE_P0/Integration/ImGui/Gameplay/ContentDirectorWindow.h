// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * ContentDirector (フェーズ進行・遷移条件・管理アクター)
 */
class ContentDirectorWindow : public ImGuiWindowBase
{
public:

	const char* GetWindowName() const override { return "ContentDirector"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::Gameplay; }
	void DrawContents() override;

};
