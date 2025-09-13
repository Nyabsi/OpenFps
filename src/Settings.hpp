#pragma once

#include <stdint.h>

class Settings {

public:
	Settings();

	[[nodiscard]] auto OverlayScale() const -> float { return overlay_scale_; }
	[[nodiscard]] auto Handedness() const -> int { return handedness_; }
	[[nodiscard]] auto Position() const -> int { return position_; }
	[[nodiscard]] auto SsScalingEnabled() const -> bool { return ss_scaling_enabled_; }
	[[nodiscard]] auto DisplayMode() const -> uint8_t { return display_mode_; }
	[[nodiscard]] auto PostProcessingEnabled() const -> bool { return post_processing_enabled_; }
	[[nodiscard]] auto ColorTemperature() const -> float { return color_temp_; }
	[[nodiscard]] auto ColorBrightness() const -> float { return color_brightness_; }

	auto Load() -> void;

	auto SetOverlayScale(float scale) -> void { 
		overlay_scale_ = scale; 
		Save();
	}

	auto SetHandedness(int handedness) -> void { 
		handedness_ = handedness;
		Save();
	}

	auto SetPosition(int position) -> void {
		position_ = position;
		Save();
	}

	auto SetSsScalingEnabled(bool enabled) -> void { 
		ss_scaling_enabled_ = enabled;
		Save();
	}

	auto SetDisplayMode(uint8_t mode) -> void {
		display_mode_ = mode; 
		Save();
	}

	auto SetPostProcessingEnabled(bool enabled) -> void {
		post_processing_enabled_ = enabled;
		Save();
	}

	auto SetColorTemparature(float temp) -> void {
		color_temp_ = temp;
		Save();
	}

	auto SetColorBrightness(float brightness) -> void {
		color_brightness_ = brightness;
		Save();
	}
private:
	auto Save() -> void;

	float overlay_scale_;
	int handedness_;
	int position_;
	bool ss_scaling_enabled_;
	uint8_t display_mode_;
	bool post_processing_enabled_;
	float color_temp_;
	float color_brightness_;
};