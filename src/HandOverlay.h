#pragma once

#include <vector>

#include <imgui.h>

#include "VrOverlay.h"
#include "Overlay.hpp"
#include "TaskMonitor.hpp"
#include "Settings.hpp"

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
    uint64_t device_id = {};
    std::string device_label = {};
    float battery_percentage = {};
};

enum Overlay_DisplayMode : uint8_t {
    Overlay_DisplayMode_None = 0, /* Not a valid mode, don't try, thanks. */
    Overlay_DisplayMode_Always = 1,
    Overlay_DisplayMode_Dashboard = 2,
};

struct OverlayTransform {
    glm::vec3 position = {};
    glm::quat rotation = {};
};

class HandOverlay : public Overlay
{
public:
    explicit HandOverlay();
    auto Initialize() -> void;

    [[nodiscard]] auto DisplayMode() const -> Overlay_DisplayMode { return display_mode_; }
    [[nodiscard]] auto OverlayScale() const -> float { return overlay_scale_; }
    [[nodiscard]] auto Handedness() const -> int { return handedness_; }
    [[nodiscard]] auto Transform() const -> OverlayTransform { return transform_; }

    auto Render() -> void override;
    auto Update() -> void override;
    auto Destroy() -> void;
    auto Reset() -> void;

    auto SetFrameTime(float refresh_rate) -> void;
private:
    auto UpdateDeviceTransform() -> void;

    TaskMonitor task_monitor_;
    Settings settings_;

    float frame_time_;
    float refresh_rate_;
    uint32_t last_pid;

    Overlay_DisplayMode display_mode_;
    OverlayTransform transform_;

    bool window_shown_;
    bool window_minimized_;
    bool keyboard_active_;

    float overlay_scale_;
    int handedness_;
    int position_;
    bool ss_scaling_enabled_;
    float ss_scale_;

    uint32_t total_dropped_frames_;
    uint32_t total_predicted_frames_;
    uint32_t total_missed_frames_;
    uint32_t total_throttled_frames_;
    uint32_t total_frames_;
    float cpu_frame_time_ms_;
    float gpu_frame_time_ms_;
	float cpu_frame_time_avg_;
	float gpu_frame_time_avg_;
    float current_fps_;
    uint8_t frame_index_;   // no HMD is >=255 (Refresh Rate) this is an safe assumption for sake of performance.
    uint32_t bottleneck_flags_;
    bool bottleneck_;
    float wireless_latency_;
    std::vector<FrameTimeInfo> cpu_frame_times_;
    std::vector<FrameTimeInfo> gpu_frame_times_;
    std::vector<TrackedDevice> tracked_devices_;

    bool color_temperature_;
    float color_channel_red_;
    float color_channel_green_;
    float color_channel_blue_;
    float color_temp_;
    float color_brightness_;
    float* colour_mask_;
};