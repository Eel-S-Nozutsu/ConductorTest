#pragma once

#ifdef IMGUI_API
#define UE_WITH_IMGUI 1
#else
#define UE_WITH_IMGUI 0
#endif //IMGUI_API

#if UE_WITH_IMGUI
#include "ImGuiModule.h"
#include <imgui.h>
#endif //WITH_IMGUI
