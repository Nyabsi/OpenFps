#pragma once

#include <string>

#include "VrOverlay.h"

struct Vulkan_Surface;

class Overlay : public VrOverlay {
public:
	[[nodiscard]] auto Surface() const noexcept -> Vulkan_Surface* { return surface_.get(); }

	Overlay(const std::string& appKey, const std::string& name, vr::VROverlayType type, int width, int height);
	virtual ~Overlay();

	virtual auto Render() -> void;
	virtual auto Update() -> void;

	auto Draw() -> void;

private:
	std::unique_ptr<Vulkan_Surface> surface_;
};