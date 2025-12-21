#include "ControllerOverlay.h"

#include <algorithm>
#include <map>
#include <thread>
#include <math.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <extension/ImGui/backends/imgui_impl_openvr.h>
#include <helper/ImHelper.h>
#include <implot.h>

#include <extension/OpenVR/VrUtils.h>

#define OVERLAY_KEY     "Nyabsi.OpenFps"
#define OVERLAY_NAME    "OpenFps Hand"
#define OVERLAY_WIDTH   420
#define OVERLAY_HEIGHT  220

static float g_overlay_width = -1.0f;
static uint32_t g_last_index = vr::k_unTrackedDeviceIndexInvalid;
static vr::ETrackedControllerRole g_overlay_handedness = vr::TrackedControllerRole_Invalid;
static glm::vec3 g_position = {};
static glm::quat g_rotation = {};

ControllerOverlay::ControllerOverlay() : Overlay(OVERLAY_KEY, OVERLAY_NAME, vr::VROverlayType_World, OVERLAY_WIDTH, OVERLAY_HEIGHT)
{
    frame_time_ = {};
    refresh_rate_ = {};
    last_pid = {};
    cpu_frame_times_ = {};
    gpu_frame_times_ = {};
    tracked_devices_ = {};
    display_mode_ = {};
    overlay_scale_ = {};
    handedness_ = {};
    position_ = {};
    ss_scaling_enabled_ = false;
    ss_scale_ = {};
    total_dropped_frames_ = {};
    total_predicted_frames_ = {};
    total_missed_frames_ = {};
    total_throttled_frames_ = {};
    total_frames_ = {};
    cpu_frame_time_ms_ = {};
    gpu_frame_time_ms_ = {};
	cpu_frame_time_avg_ = {};
	gpu_frame_time_avg_ = {};
    current_fps_ = {};
    frame_index_ = {};
    bottleneck_flags_ = {};
    bottleneck_ = false;
    wireless_latency_ = {};
    transform_ = {};
    color_temperature_ = false;
    color_channel_red_ = {};
    color_channel_green_ = {};
    color_channel_blue_ = {};
    color_temp_ = {};
    color_brightness_ = {};
    colour_mask_ = {};

    try {
        this->SetInputMethod(vr::VROverlayInputMethod_Mouse);
        this->EnableFlag(vr::VROverlayFlags_SendVRDiscreteScrollEvents);
        this->EnableFlag(vr::VROverlayFlags_EnableClickStabilization);
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize the overlay.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, "OpenFps", MB_OK);
#endif
        printf("%s\n\n", ex.what());
        std::exit(EXIT_FAILURE);
    }

    ImPlot::CreateContext();

    task_monitor_.Initialize();

    settings_.Load();

    display_mode_ = static_cast<Overlay_DisplayMode>(settings_.DisplayMode());
    overlay_scale_ = settings_.OverlayScale();
    handedness_ = settings_.Handedness();
    position_ = settings_.Position();
    ss_scaling_enabled_ = settings_.SsScalingEnabled();
    color_temperature_ = settings_.PostProcessingEnabled();
    color_temp_ = settings_.ColorTemperature();
    color_brightness_ = settings_.ColorBrightness();

    colour_mask_ = (float*)malloc(sizeof(float) * 3);
#pragma warning( push )
#pragma warning( disable : 6387 )
    memset(colour_mask_, 0x0, sizeof(float) * 3);
#pragma warning( pop )

    ss_scale_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float) * 100;
    color_channel_red_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float);
    color_channel_green_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float);
    color_channel_blue_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float);

    this->UpdateDeviceTransform();
}

auto ControllerOverlay::Render() -> bool
{
    if (!Overlay::Render())
        return false;

    if (this->DisplayMode() == Overlay_DisplayMode_Dashboard && !vr::VROverlay()->IsDashboardVisible())
        return false;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplOpenVR_NewFrame();
    ImGui::NewFrame();


    if (!vr::VROverlay()->IsDashboardVisible())
        ImHelper::DrawCursor();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImVec2(io.DisplaySize.x, io.DisplaySize.y);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(pos, ImGuiCond_Always);

    ImGui::Begin("OpenFps", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    if (!io.WantTextInput && (
        (!vr::VROverlay()->IsDashboardVisible() && (mousePos.x < 0.0f || mousePos.y < 0.0f)) ||
        (vr::VROverlay()->IsDashboardVisible() && !vr::VROverlay()->IsHoverTargetOverlay(this->Handle())))
    ) {
        ImGuiStyle& style = ImGui::GetStyle();

        GpuInfo gpu_info = {};
		ProcessInfo process_info = {};

        ImGui::Indent(10.0f);
        auto pid = GetCurrentGamePid();
        if (pid > 0) {
            // TODO: check if last_pid actually exists before doing reset, if game utilizes SteamVR Compositor GetCurrentGamePid might return wrong pid temporarily causing stat reset.
            if (last_pid != pid) {
                this->Reset();
                last_pid = pid;
            }
			process_info = task_monitor_.GetProcessInfoByPid(pid);
            gpu_info = getCurrentlyUsedGpu(process_info);
            ImGui::Text("Current Application: %s (%d)", process_info.process_name.c_str(), pid);
        }
        else {
			ImGui::Text("Current Application: SteamVR Void");
        }
        ImGui::Unindent(10.0f);

        ImGui::Spacing();

        auto avail = ImGui::GetContentRegionAvail();
        auto childSize = ImVec2((avail.x / 2) - style.FramePadding.x, (avail.y / 2) - style.FramePadding.y);

        if (ImGui::BeginChild("##metrics_info", childSize, ImGuiChildFlags_None)) {
            if (ImGui::BeginTable("##cpu_frametime", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::Indent(10.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("CPU Frametime");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f ms", cpu_frame_time_avg_);
                ImGui::EndTable();
                ImGui::Unindent(10.0f);
            }

            ImVec2 plotSize = ImGui::GetContentRegionAvail();

            static double t = 0.0;
            t += ImGui::GetIO().DeltaTime;

            const int frame_count = static_cast<int>(refresh_rate_);
            const float frame_dt = 1.0f / refresh_rate_;
            const double history = frame_count * frame_dt;

            if (ImPlot::BeginPlot("##frameplotimer", plotSize,
                ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame)) {

                ImPlot::SetupAxes(
                    nullptr,
                    nullptr,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoGridLines,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_Lock
                );

                // HACK: frame_dt * 2.0f ensures history buffer is enough to fill the entire plot
                ImPlot::SetupAxisLimits(ImAxis_X1, -(history - (frame_dt * 2.0f)), 0.0, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, frame_time_ * 2.0, ImGuiCond_Always);

                static double y_ticks[1] = { frame_time_ };
                ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 1, nullptr, false);

                for (int i = 0; i < static_cast<int>(refresh_rate_) - 1; ++i) {

                    ImVec4 color;
                    if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_Reprojecting)
                        color = Color_Orange;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_MotionSmoothingEnabled)
                        color = Color_Yellow;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_OneThirdFramePresented)
                        color = Color_Red;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_Frame_Dropped)
                        color = Color_Magenta;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_Frame_Cpu_Stalled)
                        color = Color_Purple;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_PredictedAhead)
                        color = Color_LightBlue;
                    else if (cpu_frame_times_[i].flags & FrameTimeInfo_Flags_Frame_Throttled)
                        color = Color_PinkishRed;
                    else
                        color = Color_Green;

                    color.w *= 0.5f;

                    float seg_x[2] = { -i * frame_dt, -(i + 1) * frame_dt };
                    float seg_y[2] = { cpu_frame_times_[i].frametime, cpu_frame_times_[i + 1].frametime };
                    constexpr float seg_ybase[2] = { 0.0f, 0.0f };

                    ImPlot::PushStyleColor(ImPlotCol_Fill, ImGui::ColorConvertFloat4ToU32(color));
                    ImPlot::PlotShaded(("##shaded" + std::to_string(i)).c_str(), seg_x, seg_ybase, seg_y, 2);
                    ImPlot::PopStyleColor();
                }

                ImPlot::EndPlot();
            }

            ImGui::EndChild();
        }

        ImGui::SameLine();

        if (ImGui::BeginChild("##metrics_info2", childSize, ImGuiChildFlags_None)) {
            if (ImGui::BeginTable("##gpu_frametime", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::Indent(10.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("GPU Frametime");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f ms", gpu_frame_time_avg_);
                ImGui::Unindent(10.0f);
                ImGui::EndTable();
            }

            static double t = 0.0;
            t += ImGui::GetIO().DeltaTime;

            const int frame_count = static_cast<int>(refresh_rate_);
            const float frame_dt = 1.0f / refresh_rate_;
            const double history = frame_count * frame_dt;

            ImVec2 plotSize = ImGui::GetContentRegionAvail();
            if (ImPlot::BeginPlot("Frametime Spikes GPU", plotSize, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame)) {
                ImPlot::SetupAxes(
                    nullptr,
                    nullptr,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoGridLines,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_Lock
                );

                ImPlot::SetupAxisLimits(ImAxis_X1, -(history - (frame_dt * 2.0f)), 0.0, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, frame_time_ * 2.0, ImGuiCond_Always);

                static double y_ticks[1] = { frame_time_ };
                ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 1, nullptr, false);

                for (int i = 0; i < static_cast<int>(refresh_rate_) - 1; ++i) {

                    ImVec4 color = {};

                    if (gpu_frame_times_[i].flags & FrameTimeInfo_Flags_Reprojecting)
                        color = Color_Orange;
                    else if (gpu_frame_times_[i].flags & FrameTimeInfo_Flags_MotionSmoothingEnabled)
                        color = Color_Yellow;
                    else if (gpu_frame_times_[i].flags & FrameTimeInfo_Flags_OneThirdFramePresented)
                        color = Color_Red;
                    else if (gpu_frame_times_[i].flags & FrameTimeInfo_Flags_Frame_Dropped)
                        color = Color_Magenta;
                    else
                        color = Color_Green;

                    color.w *= 0.5f;

                    float seg_x[2] = { -i * frame_dt, -(i + 1) * frame_dt };
                    float seg_y[2] = { gpu_frame_times_[i].frametime, gpu_frame_times_[i + 1].frametime };
                    constexpr float seg_ybase[2] = { 0.0f, 0.0f };

                    ImPlot::PushStyleColor(ImPlotCol_Fill, ImGui::ColorConvertFloat4ToU32(color));
                    ImPlot::PlotShaded(("##shaded" + std::to_string(i)).c_str(), seg_x, seg_ybase, seg_y, 2);
                    ImPlot::PopStyleColor();
                }

                ImPlot::EndPlot();
            }

            ImGui::EndChild();
        }

        avail = ImGui::GetContentRegionAvail();
        childSize = ImVec2((avail.x / 2) - style.FramePadding.x, avail.y - style.FramePadding.y);

        if (ImGui::BeginChild("##metrics_info3", childSize, ImGuiChildFlags_None)) {

            if (ImGui::BeginTable("##metrics_extra", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::Indent(10.0f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("CPU");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f %%", process_info.cpu.total_cpu_usage);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("FPS");
                ImGui::TableSetColumnIndex(1);
                if (bottleneck_flags_ & BottleneckSource_Flags_GPU)
                    ImGui::TextColored(Color_Orange, "%1.f", current_fps_);
                else
                    ImGui::Text("%1.f", current_fps_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Missed");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color_Red, "%d Frames", total_missed_frames_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Dropped");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color_Red, "%d Frames", total_dropped_frames_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Throttled");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color_Red, "%d Frames", total_throttled_frames_);

                ImGui::Unindent(10.0f);

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }

        ImGui::SameLine();

        if (ImGui::BeginChild("##metrics_info4", childSize, ImGuiChildFlags_None)) {



            if (ImGui::BeginTable("##metrics_extra3", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::Indent(10.0f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("GPU");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f %%", gpuPercentage(gpu_info));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("D-VRAM");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(
                    "%.0f MB (%.1f%%)",
                    gpu_info.memory.dedicated_vram_usage / (1024.0f * 1024.0f),
                    gpu_info.memory.dedicated_available > 0
                    ? (gpu_info.memory.dedicated_vram_usage * 100.0f) / gpu_info.memory.dedicated_available
                    : 0.0f
                );

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("S-VRAM");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(
                    "%.0f MB (%.1f%%)",
                    gpu_info.memory.shared_vram_usage / (1024.0f * 1024.0f),
                    gpu_info.memory.shared_available > 0
                    ? (gpu_info.memory.shared_vram_usage * 100.0f) / gpu_info.memory.shared_available
                    : 0.0f
                );

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("RAM");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(
                    "%.0f MB (%.1f%%)",
                    process_info.memory_usage / (1024.0f * 1024.0f),
                    gpu_info.memory.dedicated_available > 0
                    ? (process_info.memory_usage * 100.0f) / process_info.memory_available
                    : 0.0f
                );

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Bottleneck");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(bottleneck_ ? Color_Orange : Color_Green, "%s", bottleneck_ ? (bottleneck_flags_ == BottleneckSource_Flags_Wireless ? "Wireless" : bottleneck_flags_ == BottleneckSource_Flags_CPU ? "CPU" : "GPU") : "None");

                ImGui::Unindent(10.0f);

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }
    }
    else {
        if (ImGui::BeginTabBar("##settings")) {

            if (ImGui::BeginTabItem("Settings")) {
                if (ImGui::BeginTable("##cpu_frametime_table", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Show");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SameLine();
                    if (ImGui::Button("Dashboard")) {
                        this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        display_mode_ = Overlay_DisplayMode_Dashboard;
						settings_.SetDisplayMode(display_mode_);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Always")) {
                        this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        display_mode_ = Overlay_DisplayMode_Always;
						settings_.SetDisplayMode(display_mode_);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Scale");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SameLine();
                    if (ImGui::InputFloat("##overlay_scale", &overlay_scale_, 0.05f, 0.0f, "%.2f")) {
                        this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
						settings_.SetOverlayScale(overlay_scale_);
                    }

                    // Scale safe boundaries
                    if (overlay_scale_ < 0.10f)
                        overlay_scale_ = 0.10f;

                    if (overlay_scale_ > 1.0f)
                        overlay_scale_ = 1.0f;

                    static int selected_handedness = 0;
                    const char* handedness_types[] = { "Left Hand", "Right Hand" };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Handedness");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##handedness", handedness_types[selected_handedness])) {
                        for (int i = 0; i < IM_ARRAYSIZE(handedness_types); i++) {
                            bool is_selected = (selected_handedness == i);
                            if (ImGui::Selectable(handedness_types[i], is_selected)) {
                                selected_handedness = i;
                                this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                                handedness_ = selected_handedness + 1;
                                this->UpdateDeviceTransform();
								settings_.SetHandedness(handedness_);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    static int selected_position = 0;
                    const char* positions[] = { "Above", "Below", "Wrist" };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Position");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SameLine();

                    if (ImGui::BeginCombo("##position", positions[selected_position])) {
                        for (int i = 0; i < IM_ARRAYSIZE(positions); i++) {
                            bool is_selected = (selected_position == i);
                            if (ImGui::Selectable(positions[i], is_selected)) {
                                position_ = i;
                                this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                                this->UpdateDeviceTransform();
                                selected_position = i;
								settings_.SetPosition(position_);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Devices")) {
                ImGui::BeginChild("process_list_scroller", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

                ImGuiTableFlags flags =
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp;

                if (ImGui::BeginTable("device_list", 3, flags))
                {
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Battery %");
                    ImGui::TableHeadersRow();

                    for (auto& device : tracked_devices_)
                    {
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%llu", device.device_id);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", device.device_label.c_str());


                        ImGui::TableSetColumnIndex(2);
                        if (device.battery_percentage <= 0.2f && device.battery_percentage > 0.10f)
                            ImGui::TextColored(Color_Yellow, "%d%%", (int)(device.battery_percentage * 100));
                        else if (device.battery_percentage <= 0.10f)
                            ImGui::TextColored(Color_Red, "%d%%", (int)(device.battery_percentage * 100));
                        else
                            ImGui::Text("%d%%", (int)(device.battery_percentage * 100));
                    }

                    ImGui::EndTable();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Display")) {
                
                static float last_ss_scale = -1.0f;
                static float channel_r = 0.0f;
                static float channel_g = 0.0f;
                static float channel_b = 0.0f;

                if (ImGui::BeginTable("##display_resolution", 2, ImGuiTableFlags_SizingStretchSame)) {

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    bool enabled = ImGui::Checkbox("Enable SS Scaling", &ss_scaling_enabled_);
                    if (ss_scaling_enabled_) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Current Scale: %.0f%%", ss_scale_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        ImGui::InputFloat("##overlay_scale", &ss_scale_, 10.0f, 0.0f, "%.0f %%");

                        if (ss_scale_ < 10.0f)
                            ss_scale_ = 10.0f;

                        if (ss_scale_ > 500.0f)
                            ss_scale_ = 500.0f;

                        if (last_ss_scale != ss_scale_) {
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, ss_scale_ / 100.0f);
                            last_ss_scale = ss_scale_;
							this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }
                    }

					if (enabled != ss_scaling_enabled_) {
						settings_.SetSsScalingEnabled(ss_scaling_enabled_);
					}

                    ImGui::EndTable();
                }

                if (ImGui::BeginTable("##display_temparature", 2, ImGuiTableFlags_SizingStretchSame)) {

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Enable Color Grading", &color_temperature_);

                    if (color_temperature_) {

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Temparature: %.0f K", color_temp_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##color_temparature", &color_temp_, 1000.0f, 0.0f, "%.0f K")) {
                            this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Brightess: %.0f %%", color_brightness_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##color_temparature_strength", &color_brightness_, 10.0f, 0.0f, "%.0f %%")) {
                            this->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }

                        if (color_brightness_ < 10.0f)
                            color_brightness_ = 10.0f;

                        if (color_brightness_ > 300.0f)
                            color_brightness_ = 300.0f;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Colours: %d, %d, %d", static_cast<int>(colour_mask_[0]), static_cast<int>(colour_mask_[1]), static_cast<int>(colour_mask_[2]));
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        ImGui::ColorEdit3("##colour_overlay", colour_mask_, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoDragDrop);

                        // https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
                        float temperature = std::clamp<float>(color_temp_, 1000.0f, 40000.0f) / 100.0f;

                        if (temperature <= 66.0f) {
                            color_channel_red_ = 1.0f; // 255
                        }
                        else {
                            color_channel_red_ = (std::clamp<float>(329.698727446 * std::pow<float>((temperature - 60), -0.1332047592), 0, 255) / 255.0f);
                        }

                        if (temperature <= 66.0f) {
                            color_channel_green_ = (std::clamp<float>(99.4708025861 * std::logf(temperature) - 161.1195681661, 0, 255) / 255.0f);
                        }
                        else {
                            color_channel_green_ = (std::clamp<float>(288.1221695283 * std::pow<float>((temperature - 60), -0.0755148492), 0, 255) / 255.0f);
                        }

                        if (temperature >= 66.0f) {
                            color_channel_blue_ = 1.0f; // 255
                        }
                        else {
                            if (temperature <= 19.0f) {
                                color_channel_blue_ = 0.01f;
                            }
                            else {
                                color_channel_blue_ = (std::clamp<float>(138.5177312231 * std::logf(temperature - 10) - 305.0447927307, 0, 255) / 255.0f);
                            }
                        }

                        auto gammaCorrect = [](float channel) -> float {
                            return std::abs(channel <= 0.04045f ? channel / 12.92f : pow((channel + 0.055f) / 1.055f, 2.4f));
                        };

                        float r = gammaCorrect(color_channel_red_);
                        float g = gammaCorrect(color_channel_green_);
                        float b = gammaCorrect(color_channel_blue_);

                        if (colour_mask_[0] > 0.0f || colour_mask_[1] > 0.0f || colour_mask_[2] > 0.0f) {
                            float original_luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

                            r *= colour_mask_[0];
                            g *= colour_mask_[1];
                            b *= colour_mask_[2];

                            float tinted_luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                            if (tinted_luminance > 0.0f) {
                                float brightness_correction = original_luminance / tinted_luminance;
                                r *= brightness_correction;
                                g *= brightness_correction;
                                b *= brightness_correction;
                            }
                        }

                        float brightness_factor = (color_brightness_ / 100.0f);

                        auto toneMap = [](float c, float brightness_factor) -> float {
                            float boosted = c * brightness_factor;
                            return boosted / (1.0f + (boosted - 1.0f) * 0.5f);
                        };

                        r = toneMap(r, brightness_factor);
                        g = toneMap(g, brightness_factor);
                        b = toneMap(b, brightness_factor);

                        color_channel_red_ = r;
                        color_channel_green_ = g;
                        color_channel_blue_ = b;

                        if ((channel_r != color_channel_red_ || channel_g != color_channel_green_ || channel_b != color_channel_blue_)) {
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float, color_channel_red_);
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float, color_channel_green_);
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float, color_channel_blue_);
                            channel_r = color_channel_red_;
                            channel_g = color_channel_green_;
                            channel_b = color_channel_blue_;
                        }
                    }
                    else {
                        if (channel_r != 0.0f || channel_g != 0.0f || channel_b != 0.0f) {
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float, 1.0f);
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float, 1.0f);
                            vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float, 1.0f);
                            channel_r = 0.0f;
                            channel_g = 0.0f;
                            channel_b = 0.0f;
                        }
                    }

                    ImGui::EndTable();
                }
                
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    
    ImGui::End();
    ImGui::Render();

    return true;
}

auto ControllerOverlay::Update() -> void
{
    Overlay::Update();

    vr::Compositor_FrameTiming timings =
    {
        .m_nSize = sizeof(vr::Compositor_FrameTiming)
    };

    bool newData = vr::VRCompositor()->GetFrameTiming(&timings, 0);

    if (newData) {

        cpu_frame_time_ms_ =
            timings.m_flCompositorRenderCpuMs +
            timings.m_flPresentCallCpuMs +
            timings.m_flWaitForPresentCpuMs +
            timings.m_flClientFrameIntervalMs +
            timings.m_flSubmitFrameMs;

        gpu_frame_time_ms_ =
            timings.m_flTotalRenderGpuMs;

        uint32_t predicted_frames = VR_COMPOSITOR_ADDITIONAL_PREDICTED_FRAMES(timings);
        uint32_t throttled_frames = VR_COMPOSITOR_NUMBER_OF_THROTTLED_FRAMES(timings);

        FrameTimeInfo info_cpu = {};
        FrameTimeInfo info_gpu = {};

        if (timings.m_nNumDroppedFrames >= 1) {
            // The frame was dropped because of wireless latency.
            if (timings.m_flCompositorIdleCpuMs >= frame_time_) {
                cpu_frame_time_ms_ += timings.m_flCompositorIdleCpuMs;
            }
            if (gpu_frame_time_ms_ >= frame_time_) {
                gpu_frame_time_ms_ = frame_time_ * 2;
            }
            info_gpu.flags |= FrameTimeInfo_Flags_Frame_Dropped;
            info_cpu.flags |= FrameTimeInfo_Flags_Frame_Dropped;

        }
        else {
            if (timings.m_nNumFramePresents > 1) {
                if (timings.m_nNumMisPresented >= 2) {
                    info_gpu.flags |= FrameTimeInfo_Flags_OneThirdFramePresented;
                    if (throttled_frames >= 2)
                        info_cpu.flags |= FrameTimeInfo_Flags_Frame_Throttled;
                }
                else {
                    if (timings.m_nReprojectionFlags & vr::VRCompositor_ReprojectionAsync) {
                        if (timings.m_nReprojectionFlags & vr::VRCompositor_ReprojectionMotion) {
                            info_gpu.flags |= FrameTimeInfo_Flags_MotionSmoothingEnabled;
                        }
                        else {

                            info_gpu.flags |= FrameTimeInfo_Flags_Reprojecting;
                        }
                    }
                }
            }
            else {
                if (predicted_frames >= 1) {
                    if (cpu_frame_time_ms_ > frame_time_) {
                        if (predicted_frames >= 2)
                            info_cpu.flags |= FrameTimeInfo_Flags_Frame_Cpu_Stalled;
                        else
                            info_cpu.flags |= FrameTimeInfo_Flags_PredictedAhead;
                    }
                    else {
                        info_cpu.flags |= FrameTimeInfo_Flags_PredictedAhead;
                    }
                }
            }
        }

        info_cpu.frametime = cpu_frame_time_ms_;
        cpu_frame_times_.data()[frame_index_] = info_cpu;
        info_gpu.frametime = gpu_frame_time_ms_;
        gpu_frame_times_.data()[frame_index_] = info_gpu;

        total_missed_frames_ += timings.m_nNumMisPresented;
        total_predicted_frames_ += predicted_frames;
        total_dropped_frames_ += timings.m_nNumDroppedFrames;
        total_throttled_frames_ += throttled_frames;
        total_frames_ += timings.m_nNumFramePresents;

        if (timings.m_flTransferLatencyMs > 0.0f) {
            wireless_latency_ = timings.m_flTransferLatencyMs;
        }
        else if (timings.m_flCompositorIdleCpuMs >= 1.0f) {
            wireless_latency_ = timings.m_flCompositorIdleCpuMs;
        }
        else {
            wireless_latency_ = 0.0f;
        }

        frame_index_ = (frame_index_ + 1) % static_cast<int>(refresh_rate_);

        static BottleneckSource_Flags stable_bottleneck_flags = BottleneckSource_Flags_None;
        static BottleneckSource_Flags last_detected_flags = BottleneckSource_Flags_None;
        static int consecutive_bottleneck_frames = 0;
        static int consecutive_clear_frames = 0;

        constexpr int kTriggerThreshold = 3;
        constexpr int kClearThreshold = 10;

        BottleneckSource_Flags detected_flags = BottleneckSource_Flags_None;
        if (wireless_latency_ >= 15.0f)
            detected_flags = BottleneckSource_Flags_Wireless;
        else if ((gpu_frame_times_[frame_index_].flags & FrameTimeInfo_Flags_Reprojecting || gpu_frame_times_[frame_index_].flags & FrameTimeInfo_Flags_OneThirdFramePresented) &&
            static_cast<int>(current_fps_) != static_cast<int>(refresh_rate_))
            detected_flags = BottleneckSource_Flags_GPU;
        else if ((cpu_frame_times_[frame_index_].flags & FrameTimeInfo_Flags_Frame_Cpu_Stalled) &&
            static_cast<int>(current_fps_) != static_cast<int>(refresh_rate_))
            detected_flags = BottleneckSource_Flags_CPU;

        if (detected_flags == stable_bottleneck_flags) {
            consecutive_clear_frames = 0;
        }
        else if (detected_flags != BottleneckSource_Flags_None) {
            if (detected_flags == last_detected_flags) {
                consecutive_bottleneck_frames++;
                if (consecutive_bottleneck_frames >= kTriggerThreshold) {
                    stable_bottleneck_flags = detected_flags;
                    consecutive_bottleneck_frames = 0;
                    consecutive_clear_frames = 0;
                }
            }
            else {
                last_detected_flags = detected_flags;
                consecutive_bottleneck_frames = 1;
            }
        }
        else {
            if (stable_bottleneck_flags != BottleneckSource_Flags_None) {
                consecutive_clear_frames++;
                if (consecutive_clear_frames >= kClearThreshold) {
                    stable_bottleneck_flags = BottleneckSource_Flags_None;
                    consecutive_bottleneck_frames = 0;
                    consecutive_clear_frames = 0;
                }
            }
        }

        bottleneck_flags_ = stable_bottleneck_flags;
        bottleneck_ = (bottleneck_flags_ != BottleneckSource_Flags_None);
    }

    static double last_time = 0.0;
    if (ImGui::GetTime() - last_time >= 0.5f) {
		cpu_frame_time_avg_ = cpu_frame_time_ms_;
		gpu_frame_time_avg_ = gpu_frame_time_ms_;
        task_monitor_.Update();
        float effective_frametime_ms = std::max(
            frame_time_,
            timings.m_flCompositorRenderCpuMs +
            timings.m_flWaitForPresentCpuMs +
            timings.m_flClientFrameIntervalMs +
            timings.m_flSubmitFrameMs
        );
        current_fps_ = (effective_frametime_ms > 0.0f) ? 1000.0f / effective_frametime_ms : 0.0f;
        last_time = ImGui::GetTime();
    }

    for (uint64_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
        try {
            auto c_properties = VrTrackedDeviceProperties::FromDeviceIndex(i);
            c_properties.CheckConnection();
            int32_t type = c_properties.GetInt32(vr::Prop_DeviceClass_Int32);

            std::string name = { "-" };

            if (type == vr::TrackedDeviceClass_HMD) {
                name = "Headset";
            }

            else if (type == vr::TrackedDeviceClass_Controller) {
                int32_t type = c_properties.GetInt32(vr::Prop_ControllerRoleHint_Int32);
                name = type == vr::TrackedControllerRole_LeftHand ? "Left Controller" : "Right Controller";
            }

            else if (type == vr::TrackedDeviceClass_GenericTracker) {
                std::string type = c_properties.GetString(vr::Prop_ControllerType_String);
                name = TrackerPropStringToString(type);
            }

            if (name.length() > 0) {
                auto it = std::find_if(tracked_devices_.begin(), tracked_devices_.end(), [i](const TrackedDevice& a) { return a.device_id == i; });

                if (it == tracked_devices_.end() && c_properties.GetBool(vr::Prop_DeviceProvidesBatteryStatus_Bool)) {
                    TrackedDevice device =
                    {
                        .device_id = i,
                        .device_label = name,
                        .battery_percentage = -1.0f
                    };

                    tracked_devices_.push_back(device);
                }
                else {
                    if (it != tracked_devices_.end()) {
                        if (c_properties.GetBool(vr::Prop_DeviceProvidesBatteryStatus_Bool)) {
                            it->battery_percentage = c_properties.GetFloat(vr::Prop_DeviceBatteryPercentage_Float);
                        }
                        else {
                            tracked_devices_.erase(it);
                        }
                    }
                }
            }
        }
        catch (std::exception& ex) {
            auto it = std::find_if(tracked_devices_.begin(), tracked_devices_.end(), [i](const TrackedDevice& a) { return a.device_id == i; });
            if (it != tracked_devices_.end()) {
                tracked_devices_.erase(it);
            }
        }
    }

    switch (this->DisplayMode()) {
        case Overlay_DisplayMode_Always:
        {
            if (!this->IsVisible() && this->ShouldRender())
                this->Show();
            break;
        }
        case Overlay_DisplayMode_Dashboard:
        {
            if (this->IsVisible() && !vr::VROverlay()->IsDashboardVisible() && this->ShouldRender())
                this->Hide();

            if (!this->IsVisible() && vr::VROverlay()->IsDashboardVisible() && this->ShouldRender())
                this->Show();
            break;
        }
    }

    const auto scale = this->OverlayScale();
    if (g_overlay_width != scale) {
        this->SetWidth(scale);
        g_overlay_width = scale;
    }


    const auto transform = this->Transform();
    const auto handedness = static_cast<vr::ETrackedControllerRole>(this->Handedness());
    const auto hand_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(handedness);

    const glm::vec3 position = transform.position;
    const glm::quat rotation = transform.rotation;
    if (g_overlay_handedness != handedness || (g_position != position && g_rotation != rotation) || g_last_index == vr::k_unTrackedDeviceIndexInvalid && hand_index != vr::k_unTrackedDeviceIndexInvalid) {
        this->SetTransformDeviceRelative(handedness, position, rotation);
        g_overlay_handedness = handedness;
        g_position = position;
        g_rotation = rotation;

        if (g_last_index == vr::k_unTrackedDeviceIndexInvalid)
            g_last_index = hand_index;
    }
}

auto ControllerOverlay::Destroy() -> void
{
    free(colour_mask_);

    task_monitor_.Destroy();

    ImPlot::DestroyContext();
}

auto ControllerOverlay::Reset() -> void
{
    cpu_frame_times_.resize(static_cast<int>(refresh_rate_));
    gpu_frame_times_.resize(static_cast<int>(refresh_rate_));

    memset(cpu_frame_times_.data(), 0x0, cpu_frame_times_.size() * sizeof(FrameTimeInfo));
    memset(gpu_frame_times_.data(), 0x0, cpu_frame_times_.size() * sizeof(FrameTimeInfo));

    frame_index_ = 0;

    total_missed_frames_ = 0;
    total_predicted_frames_ = 0;
    total_dropped_frames_ = 0;
    total_throttled_frames_ = 0;
    total_frames_ = 0;
}

auto ControllerOverlay::SetFrameTime(float refresh_rate) -> void
{
    frame_time_ = 1000.0f / refresh_rate;
    refresh_rate_ = refresh_rate;

    this->Reset();
}

auto ControllerOverlay::UpdateDeviceTransform() -> void
{
    glm::vec3 position = {};
    glm::quat rotation = {};

    if (handedness_ == vr::TrackedControllerRole_LeftHand) {

        switch (position_)
        {
        case 0: {
            position = { -0.15, 0, 0.10 };
            rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        case 1: {
            position = { 0, 0, 0.25 };
            rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        case 2: {
            position = { -0.10, 0, 0.10 };
            rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        default:
            break;
        }
    }

    else if (handedness_ == vr::TrackedControllerRole_RightHand) {
        switch (position_)
        {
        case 0: {
            position = { 0.15, 0, 0.10 };
            rotation = glm::angleAxis(-glm::pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(-glm::half_pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= -glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        case 1: {
            position = { 0, 0, 0.25 };
            rotation = glm::angleAxis(-glm::pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= -glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        case 2: {
            position = { 0.10, 0, 0.10 };
            rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(glm::pi<float>(), glm::vec3(0, 0, 1)) * glm::angleAxis(-glm::half_pi<float>(), glm::vec3(0, 1, 0)) * glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1, 0, 0));
            rotation *= -glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
            rotation = glm::normalize(rotation);
            break;
        }
        default:
            break;
        }
    }

    transform_ = { position, rotation };
}