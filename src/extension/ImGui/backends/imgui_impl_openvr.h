// dear imgui: Platform backend for OpenVR
// This needs to be used along with a Renderer Backend (e.g. Vulkan)

// Implemented features:
//  [X] Platform: Virtual keyboard support
//  [X] Platform: Mouse emulation
// Missing features or Issues:
//  [ ] Platform: Touch Emulation

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once
#include <imgui.h>      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

namespace vr {
	struct VREvent_t;
}

struct ImGui_ImplOpenVR_InitInfo
{
	uintptr_t handle;
	int width;
	int height;
};

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool     ImGui_ImplOpenVR_Init(ImGui_ImplOpenVR_InitInfo* initInfo);
IMGUI_IMPL_API bool		ImGui_ImplOpenVR_ProcessOverlayEvent(const vr::VREvent_t& event);
IMGUI_IMPL_API void     ImGui_ImplOpenVR_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplOpenVR_NewFrame();

#endif // #ifndef IMGUI_DISABLE
