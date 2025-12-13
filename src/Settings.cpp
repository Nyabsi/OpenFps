#include "Settings.hpp"

#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <fstream>

Settings::Settings()
{
    overlay_scale_ = 0.15f;
    handedness_ = 1;
    position_ = 0;
    ss_scaling_enabled_ = false;
	display_mode_ = 2;
    post_processing_enabled_ = false;
	color_temp_ = 8500.0f;
	color_brightness_ = 100.0f;
}

auto Settings::Load() -> void
{
    std::string settingsPath = {};
    settingsPath += SDL_GetPrefPath("Nyabsi", "OpenFps");
    settingsPath += "settings.json";

    std::ifstream file(settingsPath);
    if (file.good()) {
        nlohmann::json j;
        file >> j;
        overlay_scale_ = j.value("overlay_scale", 0.15f);
        handedness_ = j.value("controller_handedness", 1);
        position_ = j.value("controller_position", 0);
		ss_scaling_enabled_ = j.value("ss_scaling_enabled", false);
		display_mode_ = j.value("display_mode", 2);
		post_processing_enabled_ = j.value("post_processing_enabled", false);
		color_temp_ = j.value("color_temperature", 8500.0f);
		color_brightness_ = j.value("color_brightness", 100.0f);
    }

	file.close();
}

auto Settings::Save() -> void
{
    std::string settingsPath = {};
    settingsPath += SDL_GetPrefPath("Nyabsi", "OpenFps");
    settingsPath += "settings.json";

    nlohmann::json j;
    j["overlay_scale"] = overlay_scale_;
    j["controller_handedness"] = handedness_;
	j["controller_position"] = position_;
	j["ss_scaling_enabled"] = ss_scaling_enabled_;
	j["display_mode"] = display_mode_;
	j["post_processing_enabled"] = post_processing_enabled_;
	j["color_temperature"] = color_temp_;
	j["color_brightness"] = color_brightness_;

    std::ofstream file(settingsPath);
    file << j.dump(4);

	file.close();
}
