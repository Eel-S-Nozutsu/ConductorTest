// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "ImGui/ImGuiWindowBase.h"

/**
 * 
 */
class ConductorWindow : public ImGuiWindowBase
{
public:
	const char* GetWindowName() const override { return "Conductor"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::System; }

	void DrawContents() override;

private:
};
