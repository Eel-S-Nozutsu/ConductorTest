// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiWindowBase.h"

/**
 * InputRouterComponent の内部状態を可視化するデバッグウィンドウ。
 * ・アクティブなレイヤータグ
 * ・MappingContextList の各エントリ（IMC / ActionList / LayerTag / Priority / bAutoActivate / 現在Activeか）
 * ・BeginPlay で Bind 済みの固有 IA 一覧
 * 「IMCを足したのに入力が来ない」系の切り分け用（IAがActionListに入っているか・レイヤーがpushされているか）。
 */
class InputRouterWindow : public ImGuiWindowBase
{
public:
	const char* GetWindowName() const override { return "InputRouter"; }
	EImGuiMenuCategory GetCategory() const override { return EImGuiMenuCategory::System; }

	void DrawContents() override;

private:
	// Bind済みIA一覧の絞り込み文字列
	char ActionFilter[64] = { 0 };
};

#endif // !UE_BUILD_SHIPPING
