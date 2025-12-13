#pragma once

#include <string>

#include "VrOverlay.h"

class Overlay : public VrOverlay {
public:
	Overlay(const std::string& appKey, const std::string& name, vr::VROverlayType type, int width, int height);
	virtual ~Overlay();

	virtual auto Render() -> void;
	virtual auto Update() -> void;

	auto Draw() -> void;
};