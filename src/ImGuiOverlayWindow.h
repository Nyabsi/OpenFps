#pragma once

#include <vector>

#include <imgui.h>

#include "VulkanRenderer.h"
#include "VrOverlay.h"

#define IMGUI_NORMALIZED_RGBA(r, g, b, a) ImVec4(((r) / 255.0f), ((g) / 255.0f), ((b) / 255.0f), ((a) / 255.0f))

constexpr auto Color_Green = IMGUI_NORMALIZED_RGBA(0, 255, 0, 255);         /* 0, 255, 0          */
constexpr auto Color_Orange = IMGUI_NORMALIZED_RGBA(255, 64, 0, 255);       /* 255, 128, 0        */
constexpr auto Color_LightBlue = IMGUI_NORMALIZED_RGBA(0, 128, 255, 255);   /* 0, 128, 255        */
constexpr auto Color_White = IMGUI_NORMALIZED_RGBA(255, 255, 255, 255);     /* 255, 255, 255, 255 */
constexpr auto Color_Red = IMGUI_NORMALIZED_RGBA(255, 0, 0, 255);           /* 255, 0, 0          */
constexpr auto Color_Yellow = IMGUI_NORMALIZED_RGBA(255, 255, 0, 255);      /* 255, 255, 0        */
constexpr auto Color_Magenta = IMGUI_NORMALIZED_RGBA(255, 0, 255, 255);     /* 255, 0, 255        */
constexpr auto Color_Purple = IMGUI_NORMALIZED_RGBA(128, 0, 255, 255);      /* 128, 0, 255        */

enum FrameTimeInfo_Flags : uint32_t {
    FrameTimeInfo_Flags_None = 0,
    FrameTimeInfo_Flags_Reprojecting = 1 << 0,              // Could either pose extrapolation (CPU) or reprojection (GPU)
    FrameTimeInfo_Flags_PredictedAhead = 1 << 1,
    FrameTimeInfo_Flags_MotionSmoothingEnabled = 1 << 2,
    FrameTimeInfo_Flags_OneThirdFramePresented = 1 << 3,
    FrameTimeInfo_Flags_Frame_Dropped = 1 << 4,
    FrameTimeInfo_Flags_Frame_Cpu_Stalled = 1 << 5
};

enum BottleneckSource_Flags : uint32_t {
    BottleneckSource_Flags_None = 0,
    BottleneckSource_Flags_CPU = 1 << 0,
    BottleneckSource_Flags_GPU = 1 << 1,
    BottleneckSource_Flags_Wireless = 1 << 2
};

struct alignas(8) FrameTimeInfo
{
    float frametime = { 0.0f };
    uint32_t flags = { FrameTimeInfo_Flags_None };
};

struct TrackedDevice {
    uint64_t device_id = { 0 };
    std::string device_label = {};
    float battery_percentage = {};
};

enum Overlay_DisplayMode {
    Overlay_DisplayMode_None = 0, /* Not a valid mode, don't try, thanks. */
    Overlay_DisplayMode_Always = 1,
    Overlay_DisplayMode_Dashboard = 2,
};

struct OverlayTransform {
    glm::vec3 position = {};
    glm::quat rotation = {};
};

class ImGuiOverlayWindow
{
public:
    explicit ImGuiOverlayWindow();
    auto Initialize(VulkanRenderer*& renderer, VrOverlay*& overlay, int width, int height) -> void;

    [[nodiscard]] auto OverlayData() -> Vulkan_Overlay* { return reinterpret_cast<Vulkan_Overlay*>(&overlay_data_); };
    [[nodiscard]] auto DisplayMode() const -> Overlay_DisplayMode { return display_mode_; }
    [[nodiscard]] auto OverlayScale() const -> float { return overlay_scale_; }
    [[nodiscard]] auto Handedness() const -> int { return handedness_; }
    [[nodiscard]] auto Transform() const -> OverlayTransform { return transform_; }

    auto Draw() -> void;
    auto Destroy() -> void;

    auto SetFrameTime(float refresh_rate) -> void;
private:
    auto UpdateDeviceTransform() -> void;

    Vulkan_Overlay overlay_data_;
    float frame_time_;
    float refresh_rate_;
    VrOverlay* overlay_;
    std::vector<FrameTimeInfo> cpu_frametimes_;
    std::vector<FrameTimeInfo> gpu_frametimes_;
    std::vector<TrackedDevice> tracked_devices_;
    Overlay_DisplayMode display_mode_;
    float overlay_scale_;
    int handedness_;
    int position_;
    bool ss_scaling_enabled_;
    float ss_scale_;
    uint32_t total_dropped_frames_;
    uint32_t total_predicted_frames_;
    uint32_t total_missed_frames_;
    uint32_t total_frames_;
    float cpu_frametime_ms_;
    float gpu_frametime_ms_;
    float current_fps_;
    uint8_t frame_index_;   // no HMD is >=255 (Refresh Rate) this is an safe assumption for sake of performance.
    uint32_t bottleneck_flags_;
    bool bottleneck_;
    float wireless_latency_;
    OverlayTransform transform_;
};