#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <stdexcept>
#include <format>

#include <openvr.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace vr {
    enum VROverlayType {
        VROverlayType_None = 0,
        VROverlayType_World = 1,
        VROverlayType_Dashboard = 2,
        VROverlayType_Subview = 3,
    };
}

class VrOverlay {
public:
    explicit VrOverlay()
        : handle(vr::k_ulOverlayHandleInvalid),
        thumbnail_handle(vr::k_ulOverlayHandleInvalid),
        type_(vr::VROverlayType_None) {}

    [[nodiscard]] auto Handle() const -> vr::VROverlayHandle_t { return handle; }

    [[maybe_unused]] auto Create(vr::VROverlayType type, const char* key, const char* name) -> void {
        type_ = type;
        if (type == vr::VROverlayType_World) {
            vr::EVROverlayError result = vr::VROverlay()->CreateOverlay(key, name, &handle);
            if (result > vr::VROverlayError_None)
                throw std::runtime_error(
                    std::format("Failed to create world overlay \"{}\" (\"{}\"): {}", name, key, static_cast<int>(result))
                );
        }
        if (type == vr::VROverlayType_Dashboard) {
            vr::EVROverlayError result = vr::VROverlay()->CreateDashboardOverlay(key, name, &handle, &thumbnail_handle);
            if (result > vr::VROverlayError_None)
                throw std::runtime_error(
                    std::format("Failed to create dashboard overlay \"{}\" (\"{}\"): {}", name, key, static_cast<int>(result))
                );
        }
        if (type == vr::VROverlayType_Subview) {
            throw std::runtime_error(std::format("Not implemented"));
        }
    }

    [[maybe_unused]] auto SetThumbnail(const std::string& path) const -> void {
        if (type_ == vr::VROverlayType_Dashboard) {
            vr::EVROverlayError result = vr::VROverlay()->SetOverlayFromFile(thumbnail_handle, path.data());
            if (result > vr::VROverlayError_None)
                throw std::runtime_error(
                    std::format("Failed to set overlay input method \"{}\": {}", path, static_cast<int>(result))
                );
            return;
        }
        throw std::runtime_error(std::format("You should only call SetThumbnail when the overlay type is VROverlayType_Dashboard"));
    }

    [[maybe_unused]] auto SetInputMethod(vr::VROverlayInputMethod method) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayInputMethod(handle, method);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(
                std::format("Failed to set overlay input method \"{}\": {}", static_cast<int>(method), static_cast<int>(result))
            );
    }

    [[maybe_unused]] auto FlagEnabled(vr::VROverlayFlags flag) const -> bool {
        bool enabled = {};
        vr::EVROverlayError result = vr::VROverlay()->GetOverlayFlag(handle, flag, &enabled);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(
                std::format("Failed to check if overlay flag is enabled \"{}\": {}", static_cast<int>(flag), static_cast<int>(result))
            );
        return enabled;
    }

    [[maybe_unused]] auto EnableFlag(vr::VROverlayFlags flag) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayFlag(handle, flag, true);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(
                std::format("Failed to enable overlay flag \"{}\": {}", static_cast<int>(flag), static_cast<int>(result))
            );
    }

    [[maybe_unused]] auto DisableFlag(vr::VROverlayFlags flag) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayFlag(handle, flag, false);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(
                std::format("Failed to disable overlay flag \"{}\": {}", static_cast<int>(flag), static_cast<int>(result))
            );
    }

    [[maybe_unused]] auto SetWidth(float width) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayWidthInMeters(handle, width);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(std::format("Failed to set overlay width \"{}\": {}", width, static_cast<int>(result)));
    }

    [[maybe_unused]] auto SetTexture(const vr::Texture_t& texture) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayTexture(handle, &texture);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(std::format("Failed to set texture {}", static_cast<int>(result)));
    }

    [[maybe_unused]] auto SetMouseScale(float x, float y) const -> void {
        vr::HmdVector2_t scale = {x, y};
        vr::EVROverlayError result = vr::VROverlay()->SetOverlayMouseScale(handle, &scale);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(std::format("Failed to set mouse scale ({}, {}) {}", x, y, static_cast<int>(result)));
    }

    [[maybe_unused]] auto ShowKeyboard(vr::EGamepadTextInputMode mode, bool multi_line = false) -> void {
        vr::EVROverlayError result = vr::VROverlay()->ShowKeyboardForOverlay(handle, mode, multi_line ? vr::k_EGamepadTextInputLineModeMultipleLines : vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Minimal | vr::KeyboardFlag_HideDoneKey, "OpenVR Overlay Provided Virtual Keyboard", 1, "", NULL);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(std::format("Failed to show keyboard {}", static_cast<int>(result)));
    }

    [[maybe_unused]] auto SetTransformWorldRelative(vr::ETrackingUniverseOrigin origin, const glm::vec3& position, const glm::quat& rotation) const -> void {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation);
        
        vr::HmdMatrix34_t m = {};
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                m.m[row][col] = transform[col][row];
            }
            m.m[row][3] = transform[3][row];
        }

        vr::VROverlay()->SetOverlayTransformAbsolute(handle, origin, &m);
    }

    [[maybe_unused]] auto SetTransformDeviceRelative(vr::ETrackedControllerRole role, const glm::vec3& position, const glm::quat& rotation) const -> void {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation);

        vr::HmdMatrix34_t m = {};
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                m.m[row][col] = transform[col][row];
            }
            m.m[row][3] = transform[3][row];
        }

        vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(handle, vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(role), &m);
    }

    // TODO: SetOverlayTransformTrackedDeviceComponent if needed

    [[maybe_unused]] auto TriggerLaserMouseHapticVibration(float duration, float frequency, float amplitude) const -> void {
        vr::EVROverlayError result = vr::VROverlay()->TriggerLaserMouseHapticVibration(handle, duration, frequency, amplitude);
        if (result > vr::VROverlayError_None)
            throw std::runtime_error(std::format("Failed to show keyboard {}", static_cast<int>(result)));
    }

    [[maybe_unused]] auto HideKeyboard() -> void {
        vr::VROverlay()->HideKeyboard();
    }

    [[maybe_unused]] auto IsVisible() const -> bool {
        return vr::VROverlay()->IsOverlayVisible(handle);
    }

    [[maybe_unused]] auto Show() const -> void {
        vr::VROverlay()->ShowOverlay(handle);
    }

    [[maybe_unused]] auto Hide() const -> void {
        vr::VROverlay()->HideOverlay(handle);
    }

    [[maybe_unused]] auto Destroy() const -> void {
        vr::VROverlay()->DestroyOverlay(handle);
    }

private:
    vr::VROverlayHandle_t handle;
    vr::VROverlayHandle_t thumbnail_handle;
    vr::VROverlayType type_;
};