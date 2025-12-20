#pragma once

#include <string>
#include <imgui.h>
#include "VrOverlay.h"

struct Vulkan_Surface;

class Overlay : public VrOverlay {
public:
	[[nodiscard]] auto Surface() const noexcept -> Vulkan_Surface* { return surface_.get(); }
	[[nodiscard]] auto Context() const noexcept -> ImGuiContext* { return context_; }
	[[nodiscard]] auto ShouldRender() const noexcept -> bool { return !keyboard_global_show_ && type_ == vr::VROverlayType_World }

	Overlay(const std::string& appKey, const std::string& name, vr::VROverlayType type, int width, int height);
	virtual ~Overlay();

	virtual auto Render() -> bool;
	virtual auto Update() -> void;

	auto Draw() -> void;

private:
	std::unique_ptr<Vulkan_Surface> surface_;
	vr::VROverlayType type_;
	ImGuiContext* context_;
	bool keyboard_global_show_;
};