// Copyright (c) 2026, Syunsuke Nodutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"

/**
 * 汎用ImGuiログウィンドウ
 */
struct FImGuiLogWindow
{
	FImGuiLogWindow();

	void Clear();
	void AddLog(const FString& Message);
	void Draw(const char* Title, bool* p_open = nullptr);
	void DrawContents();

private:

	ImGuiTextBuffer Buffer;
	ImGuiTextFilter Filter;
	TArray<int32> LineOffsets; // 行ごとの開始位置
	bool AutoScroll = true;

};
