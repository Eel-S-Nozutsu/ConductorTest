// Copyright (c) 2026, S.Rokumoto EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Users/rokumoto/SmxTestWindow.h"

#if !UE_BUILD_SHIPPING

namespace app
{
	void my_window_abc();
	void my_window_shop();
	void my_window_rokumoto_vending_machine();
}

void SmxTestWindow::DrawContents()
{
	ImGui::Text("Hello, ImGui!");

	app::my_window_abc();
	app::my_window_shop();
	app::my_window_rokumoto_vending_machine();
}

#endif // !UE_BUILD_SHIPPING
