#pragma once

#include <imgui.h>
#include <openvr.h>

#define IMGUI_NORMALIZED_RGBA(r, g, b, a) ImVec4(((r) / 255.0f), ((g) / 255.0f), ((b) / 255.0f), ((a) / 255.0f))

constexpr auto Color_Green = IMGUI_NORMALIZED_RGBA(0, 255, 0, 255);         /* 0, 255, 0          */
constexpr auto Color_Orange = IMGUI_NORMALIZED_RGBA(255, 64, 0, 255);       /* 255, 128, 0        */
constexpr auto Color_LightBlue = IMGUI_NORMALIZED_RGBA(0, 128, 255, 255);   /* 0, 128, 255        */
constexpr auto Color_White = IMGUI_NORMALIZED_RGBA(255, 255, 255, 255);     /* 255, 255, 255, 255 */
constexpr auto Color_Red = IMGUI_NORMALIZED_RGBA(255, 0, 0, 255);           /* 255, 0, 0          */
constexpr auto Color_PinkishRed = IMGUI_NORMALIZED_RGBA(241, 12, 69, 255);  /* 241, 12, 69        */
constexpr auto Color_Yellow = IMGUI_NORMALIZED_RGBA(255, 255, 0, 255);      /* 255, 255, 0        */
constexpr auto Color_Magenta = IMGUI_NORMALIZED_RGBA(255, 0, 255, 255);     /* 255, 0, 255        */
constexpr auto Color_Purple = IMGUI_NORMALIZED_RGBA(128, 0, 255, 255);      /* 128, 0, 255        */

namespace ImHelper {
    inline void DrawCursor()
    {
        ImGuiIO& io = ImGui::GetIO();

        if (io.MousePos.x < 0.0f || io.MousePos.y < 0.0f)
            return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddCircleFilled(io.MousePos, 6.0f, ImGui::ColorConvertFloat4ToU32(Color_LightBlue));
    }
}