// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

// IMGUIモジュールが有効かどうか
#ifdef IMGUI_API
#define UE_WITH_IMGUI 1
#else
#define UE_WITH_IMGUI 0
#endif //IMGUI_API


// IMGUIモジュールが有効の時に、必要なincludeを行う
#if UE_WITH_IMGUI
#include "ImGuiModule.h"
// 他に追加したいImGui関連のincludeはここに追加していく
#include <imgui.h>
#endif //WITH_IMGUI
