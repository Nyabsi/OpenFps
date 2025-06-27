#include "ImGuiOverlayWindow.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "backends/imgui_impl_openvr.h"

#include <math.h>
#include <implot.h>
#include <algorithm>

#include "VrUtils.h"
#include <map>

ImGuiOverlayWindow::ImGuiOverlayWindow()
{
    frame_time_ = {};
    refresh_rate_ = {};
    overlay_ = nullptr;
    cpu_frametimes_ = {};
    gpu_frametimes_ = {};
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
    total_frames_ = {};
    cpu_frametime_ms_ = {};
    gpu_frametime_ms_ = {};
    current_fps_ = {};
    frame_index_ = {};
    bottleneck_flags_ = {};
    bottleneck_ = false;
    wireless_latency_ = {};
    transform_ = {};
    color_temparature_ = false;
    color_channel_red_ = {};
    color_channel_green_ = {};
    color_channel_blue_ = {};
    color_temp_ = {};
    color_brightness_ = {};
    color_contrast_ = {};
    colour_mask_ = {};
}

auto ImGuiOverlayWindow::Initialize(VulkanRenderer*& renderer, VrOverlay*& overlay, int width, int height) -> void
{
    overlay_ = overlay;
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.ConfigFlags  |= ImGuiConfigFlags_IsSRGB;

    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 10.0f;
    style.Alpha = 0.5f;

    // style.Colors[ImGuiCol_WindowBg] = ImVec4();

    style.ScaleAllSizes(1.0f);
    style.FontScaleDpi = 1.0f;

    if (io.ConfigFlags & ImGuiConfigFlags_IsSRGB) {
        // hack: ImGui doesn't handle sRGB colour spaces properly so convert from Linear -> sRGB
        // https://github.com/ocornut/imgui/issues/8271#issuecomment-2564954070
        // remove when these are merged:
        //  https://github.com/ocornut/imgui/pull/8110
        //  https://github.com/ocornut/imgui/pull/8111
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
            ImVec4& col = style.Colors[i];
            col.x = col.x <= 0.04045f ? col.x / 12.92f : pow((col.x + 0.055f) / 1.055f, 2.4f);
            col.y = col.y <= 0.04045f ? col.y / 12.92f : pow((col.y + 0.055f) / 1.055f, 2.4f);
            col.z = col.z <= 0.04045f ? col.z / 12.92f : pow((col.z + 0.055f) / 1.055f, 2.4f);
        }
    }

    ImGui_ImplOpenVR_InitInfo openvr_init_info =
    {
        .handle = overlay->Handle(),
        .width = width,
        .height = height
    };

    ImGui_ImplOpenVR_Init(&openvr_init_info);

    VkSurfaceFormatKHR surface_format =
    {
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
    };

    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    ImGui_ImplVulkan_InitInfo init_info = {
        .ApiVersion = VK_API_VERSION_1_3,
        .Instance = renderer->Instance(),
        .PhysicalDevice = renderer->PhysicalDevice(),
        .Device = renderer->Device(),
        .QueueFamily = renderer->QueueFamily(),
        .Queue = renderer->Queue(),
        .DescriptorPool = renderer->DescriptorPool(),
        .RenderPass = VK_NULL_HANDLE,
        .MinImageCount = 16,
        .ImageCount = 16,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache = renderer->PipelineCache(),
        .Subpass = 0,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = pipeline_rendering_create_info,
        .Allocator = renderer->Allocator(),
        .CheckVkResultFn = nullptr,
    };

    ImGui_ImplVulkan_Init(&init_info);
    renderer->SetupOverlay(width, height, surface_format);

    cpu_frametimes_.resize(static_cast<int>(refresh_rate_));
    gpu_frametimes_.resize(static_cast<int>(refresh_rate_));

    memset(cpu_frametimes_.data(), 0x0, cpu_frametimes_.size() * sizeof(FrameTimeInfo));
    memset(gpu_frametimes_.data(), 0x0, cpu_frametimes_.size() * sizeof(FrameTimeInfo));

    display_mode_ = Overlay_DisplayMode_Dashboard; // TODO: settings
    overlay_scale_ = 0.15f; // TODO: settings
    handedness_ = 1; // TODO: settings
    position_ = 0;
    color_temp_ = 15000;
    color_brightness_ = 100;
    color_contrast_ = 100;
    float color_mask[3] = { 0.0f, 0.0f, 0.0f };
    colour_mask_ = color_mask;

    ss_scale_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float) * 100;
    color_channel_red_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float);
    color_channel_green_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float);
    color_channel_blue_ = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float);

    this->UpdateDeviceTransform();
}

auto ImGuiOverlayWindow::Draw() -> void
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplOpenVR_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImVec2(io.DisplaySize.x, io.DisplaySize.y);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(pos, ImGuiCond_Always);

    ImGui::Begin("OpenFps", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    if ((!vr::VROverlay()->IsHoverTargetOverlay(overlay_->Handle()) && !io.WantTextInput) || !vr::VROverlay()->IsDashboardVisible()) {

        vr::Compositor_FrameTiming timings =
        {
            .m_nSize = sizeof(vr::Compositor_FrameTiming)
        };

        bool newData = vr::VRCompositor()->GetFrameTiming(&timings, 0);

        if (newData) {

            cpu_frametime_ms_ =
                timings.m_flCompositorRenderCpuMs +
                timings.m_flPresentCallCpuMs +
                timings.m_flWaitForPresentCpuMs +
                timings.m_flClientFrameIntervalMs + // same as m_flNewPosesReadyMs - m_flNewFrameReadyMs
                timings.m_flSubmitFrameMs;

            gpu_frametime_ms_ =
                timings.m_flTotalRenderGpuMs;

            uint32_t predicted_frames = VR_COMPOSITOR_ADDITIONAL_PREDICTED_FRAMES(timings);
            uint32_t throttled_frames = VR_COMPOSITOR_NUMBER_OF_THROTTLED_FRAMES(timings);

            FrameTimeInfo info_cpu = {};
            FrameTimeInfo info_gpu = {};

            if (timings.m_nNumDroppedFrames >= 1) {
                // The frame was dropped because of wireless latency.
                if (timings.m_flCompositorIdleCpuMs >= frame_time_) {
                    cpu_frametime_ms_ += timings.m_flCompositorIdleCpuMs;
                }
                if (gpu_frametime_ms_ >= frame_time_) {
                    gpu_frametime_ms_ = frame_time_ * 2;
                }
                info_gpu.flags |= FrameTimeInfo_Flags_Frame_Dropped;
                info_cpu.flags |= FrameTimeInfo_Flags_Frame_Dropped;

            }
            else {
                if (timings.m_nNumFramePresents > 1) {
                    if (timings.m_nNumMisPresented >= 2) {
                        info_gpu.flags |= FrameTimeInfo_Flags_OneThirdFramePresented;
                        if (throttled_frames >= 2)
                            info_cpu.flags |= FrameTimeInfo_Flags_OneThirdFramePresented; // TODO: colour code throttle
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
                        if (cpu_frametime_ms_ > frame_time_) {
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

            info_cpu.frametime = cpu_frametime_ms_;
            cpu_frametimes_.data()[frame_index_] = info_cpu;
            info_gpu.frametime = gpu_frametime_ms_;
            gpu_frametimes_.data()[frame_index_] = info_gpu;

            total_missed_frames_ += timings.m_nNumMisPresented;
            total_predicted_frames_ += predicted_frames;
            total_dropped_frames_ += timings.m_nNumDroppedFrames;

            if (timings.m_nNumFramePresents == 1)
                total_frames_ += timings.m_nNumFramePresents;

            if (timings.m_flTransferLatencyMs > 0.0f) {
                wireless_latency_ = timings.m_flTransferLatencyMs;
            }
            else if (timings.m_flCompositorIdleCpuMs >= 1.0f) {
                wireless_latency_ = std::roundf(timings.m_flCompositorIdleCpuMs);
            }
            else {
                wireless_latency_ = 0.0f;
            }

            float effective_frametime_ms = {};

            // Only GPU reprojection guarantees that the frame is consistently halfed.
            if (bottleneck_flags_ & BottleneckSource_Flags_GPU)
                effective_frametime_ms = frame_time_ * 2.0f;
            else
                effective_frametime_ms = std::max(frame_time_, gpu_frametime_ms_);

            current_fps_ = (effective_frametime_ms > 0.0f) ? 1000.0f / effective_frametime_ms : 0.0f; 

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
            else if ((gpu_frametimes_[frame_index_].flags & FrameTimeInfo_Flags_Reprojecting || gpu_frametimes_[frame_index_].flags & FrameTimeInfo_Flags_OneThirdFramePresented) &&
                static_cast<int>(current_fps_) != static_cast<int>(refresh_rate_))
                detected_flags = BottleneckSource_Flags_GPU;
            else if ((cpu_frametimes_[frame_index_].flags & FrameTimeInfo_Flags_Frame_Cpu_Stalled) &&
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

        static double last_time = -1.0f;
        static double current_time = ImGui::GetTime();

        // PS. bad.
        if (true) {
            for (uint64_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                try {
                    auto c_properties = VrTrackedDeviceProperties::FromDeviceIndex(i);
                    c_properties.CheckConnection();
                    int32_t type = c_properties.GetInt32(vr::Prop_DeviceClass_Int32);

                    std::string name = {};

                    if (type == vr::TrackedDeviceClass_HMD) {
                        name = "Headset";
                    }

                    else if (type == vr::TrackedDeviceClass_Controller) {
                        int32_t type = c_properties.GetInt32(vr::Prop_ControllerRoleHint_Int32);
                        name = type == vr::TrackedControllerRole_LeftHand ? "Left Controller" : "Right Controller";
                    }

                    else if (type == vr::TrackedDeviceClass_GenericTracker) {
                        std::string name_unformatted = c_properties.GetString(vr::Prop_ControllerType_String);

                        if (name_unformatted.contains("vive_tracker_left_foot"))
                            name = "Left Foot";
                        else if (name_unformatted.contains("vive_tracker_right_foot"))
                            name = "Right Foot";
                        else if (name_unformatted.contains("vive_tracker_left_shoulder"))
                            name = "Left Shoulder";
                        else if (name_unformatted.contains("vive_tracker_right_shoulder"))
                            name = "Right Shoulder";
                        else if (name_unformatted.contains("vive_tracker_left_elbow"))
                            name = "Left Elbow";
                        else if (name_unformatted.contains("vive_tracker_right_elbow"))
                            name = "Right Elbow";
                        else if (name_unformatted.contains("vive_tracker_left_knee"))
                            name = "Left Knee";
                        else if (name_unformatted.contains("vive_tracker_right_knee"))
                            name = "Right Knee";
                        else if (name_unformatted.contains("vive_tracker_waist"))
                            name = "Waist";
                        else if (name_unformatted.contains("vive_tracker_chest"))
                            name = "Chest";
                        else if (name_unformatted.contains("vive_tracker_camera"))
                            name = "Camera";
                        else if (name_unformatted.contains("vive_tracker_keyboard"))
                            name = "Keyboard";
                        else if (name_unformatted.contains("vive_tracker_handed"))
                            name = "Handed Tracker";
                        else
                            name = "Generic Tracker";
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
        }


        size_t splitIndex = tracked_devices_.size() / 2;
        auto tracker_batteries_low = std::vector(tracked_devices_.begin(), tracked_devices_.begin() + splitIndex);
        auto tracker_batteries_high = std::vector(tracked_devices_.begin() + splitIndex, tracked_devices_.end());

        ImGuiStyle& style = ImGui::GetStyle();

        auto avail = ImGui::GetContentRegionAvail();
        auto childSize = ImVec2((avail.x / 2) - style.FramePadding.x, (avail.y / 2.5) - style.FramePadding.y);


        if (ImGui::BeginChild("##metrics_info", childSize, ImGuiChildFlags_None)) {
            if (ImGui::BeginTable("##cpu_frametime", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("CPU Frametime");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f ms", cpu_frametime_ms_);

                ImGui::EndTable();
            }

            ImVec2 plotSize = ImGui::GetContentRegionAvail();
            if (ImPlot::BeginPlot("##frameplotimer", plotSize, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame)) {
                ImPlot::SetupAxes(
                    nullptr,
                    nullptr,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoGridLines,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_Lock
                );

                static double y_ticks[1] = { frame_time_ };
                ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 1, nullptr, false);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, frame_time_ * 2, ImGuiCond_Always);

                for (int i = 0; i < static_cast<int>(refresh_rate_) - 1; ++i) {

                    ImVec4 color = {};

                    if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_Reprojecting)
                        color = Color_Orange;
                    else if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_MotionSmoothingEnabled)
                        color = Color_Yellow;
                    else if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_OneThirdFramePresented)
                        color = Color_Red;
                    else if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_Frame_Dropped)
                        color = Color_Magenta;
                    else if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_Frame_Cpu_Stalled)
                        color = Color_Purple;
                    else if (cpu_frametimes_[i].flags & FrameTimeInfo_Flags_PredictedAhead)
                        color = Color_LightBlue;
                    else
                        color = Color_Green;

                    color.w *= 0.5f;

                    float seg_x[2] = { static_cast<float>(i), static_cast<float>(i + 1) };
                    float seg_y[2] = { cpu_frametimes_[i].frametime, cpu_frametimes_[i + 1].frametime };
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
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("GPU Frametime");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f ms", gpu_frametime_ms_);

                ImGui::EndTable();
            }

            ImVec2 plotSize = ImGui::GetContentRegionAvail();
            if (ImPlot::BeginPlot("Frametime Spikes GPU", plotSize, ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame)) {
                ImPlot::SetupAxes(
                    nullptr,
                    nullptr,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoGridLines,
                    ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_Lock
                );

                static double y_ticks[1] = { frame_time_ };
                ImPlot::SetupAxisTicks(ImAxis_Y1, y_ticks, 1, nullptr, false);
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, frame_time_ * 2, ImGuiCond_Always);

                for (int i = 0; i < static_cast<int>(refresh_rate_) - 1; ++i) {

                    ImVec4 color = {};

                    if (gpu_frametimes_[i].flags & FrameTimeInfo_Flags_Reprojecting)
                        color = Color_Orange;
                    else if (gpu_frametimes_[i].flags & FrameTimeInfo_Flags_MotionSmoothingEnabled)
                        color = Color_Yellow;
                    else if (gpu_frametimes_[i].flags & FrameTimeInfo_Flags_OneThirdFramePresented)
                        color = Color_Red;
                    else if (gpu_frametimes_[i].flags & FrameTimeInfo_Flags_Frame_Dropped)
                        color = Color_Magenta;
                    else
                        color = Color_Green;

                    color.w *= 0.5f;

                    float seg_x[2] = { static_cast<float>(i), static_cast<float>(i + 1) };
                    float seg_y[2] = { gpu_frametimes_[i].frametime, gpu_frametimes_[i + 1].frametime };
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
        childSize = ImVec2((avail.x / 2) - style.FramePadding.x, (avail.y / 2.5) - style.FramePadding.y);

        if (ImGui::BeginChild("##metrics_info3", childSize, ImGuiChildFlags_None)) {

            if (ImGui::BeginTable("##metrics_extra", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Missed");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color_Red, "%d Frames", total_missed_frames_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("FPS");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(bottleneck_flags_ & BottleneckSource_Flags_GPU ? Color_Orange : Color_White, "%1.f", current_fps_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Total");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d Frames", total_frames_);

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }

        ImGui::SameLine();

        if (ImGui::BeginChild("##metrics_info4", childSize, ImGuiChildFlags_None)) {



            if (ImGui::BeginTable("##metrics_extra2", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Dropped");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color_Red, "%d Frames", total_dropped_frames_);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Bottleneck");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(bottleneck_ ? Color_Orange : Color_Green, "%s", bottleneck_ ? (bottleneck_flags_ == BottleneckSource_Flags_Wireless ? "Wireless" : bottleneck_flags_ == BottleneckSource_Flags_CPU ? "CPU" : "GPU") : "None");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Transmit Latency");
                ImGui::TableSetColumnIndex(1);
                if (wireless_latency_ > 0.0f)
                    ImGui::TextColored(Color_LightBlue, "%.1f ms", wireless_latency_);
                else
                    ImGui::TextColored(Color_Magenta, "N/A");

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }

        avail = ImGui::GetContentRegionAvail();
        childSize = ImVec2((avail.x / 2) - style.FramePadding.x, avail.y - style.FramePadding.y);
        auto childSizeTotal = ImVec2(avail.x, avail.y);

        if (ImGui::BeginChild("##battery_section", childSizeTotal, ImGuiChildFlags_None)) {
            if (ImGui::BeginChild("##battery_section_high", childSize, ImGuiChildFlags_None)) {
                if (ImGui::BeginTable("##batteries_high", 2, ImGuiTableFlags_SizingStretchProp)) {
                    for (auto& tracker : tracker_batteries_high) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("(%d) %s", tracker.device_id, tracker.device_label.c_str());
                        ImGui::TableSetColumnIndex(1);
                        if (tracker.battery_percentage <= 0.2f)
                            ImGui::TextColored(Color_Red, "%d%%", static_cast<int>(tracker.battery_percentage * 100));
                        else
                            ImGui::Text("%d%%", static_cast<int>(tracker.battery_percentage * 100));
                    }
                    ImGui::EndTable();
                }

                ImGui::EndChild();
            }

            ImGui::SameLine();

            if (ImGui::BeginChild("##battery_section_low", childSize, ImGuiChildFlags_None)) {
                if (ImGui::BeginTable("##batteries_low", 2, ImGuiTableFlags_SizingStretchProp)) {
                    for (auto& tracker : tracker_batteries_low) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("(%d) %s", tracker.device_id, tracker.device_label.c_str());
                        ImGui::TableSetColumnIndex(1);
                        if (tracker.battery_percentage <= 0.2f)
                            ImGui::TextColored(Color_Red, "%d%%", static_cast<int>(tracker.battery_percentage * 100));
                        else
                            ImGui::Text("%d%%", static_cast<int>(tracker.battery_percentage * 100));
                    }
                    ImGui::EndTable();
                }

                ImGui::EndChild();
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
                        overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        display_mode_ = Overlay_DisplayMode_Dashboard;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Always")) {
                        overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        display_mode_ = Overlay_DisplayMode_Always;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Scale");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SameLine();
                    if (ImGui::InputFloat("##overlay_scale", &overlay_scale_, 0.05f, 0.0f, "%.2f")) {
                        overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
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
                                overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                                handedness_ = selected_handedness + 1;
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
                                overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                                this->UpdateDeviceTransform();
                                selected_position = i;
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::EndTable();
                }

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
                    ImGui::Checkbox("Enable SS Scaling", &ss_scaling_enabled_);
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
                        }
                    }

                    ImGui::EndTable();
                }

                if (ImGui::BeginTable("##display_temparature", 2, ImGuiTableFlags_SizingStretchSame)) {

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("Enable Color Grading", &color_temparature_);

                    if (color_temparature_) {

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Temparature: %.0f K", color_temp_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##color_temparature", &color_temp_, 1000.0f, 0.0f, "%.0f K")) {
                            overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(color_brightness_ > 100.0f ? Color_Red : Color_White, color_brightness_ > 100.0f ? "Brightess (OVERDRIVE): %.0f %%" : "Brightess: %.0f %%", color_brightness_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##color_temparature_strength", &color_brightness_, 10.0f, 0.0f, "%.0f %%")) {
                            overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }

                        if (color_brightness_ < 10.0f)
                            color_brightness_ = 10.0f;

                        if (color_brightness_ > 200.0f)
                            color_brightness_ = 200.0f;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Contrast: %.0f %%", color_contrast_);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::SameLine();
                        if (ImGui::InputFloat("##color_contrast", &color_contrast_, 5.0f, 0.0f, "%.0f %%")) {
                            overlay_->TriggerLaserMouseHapticVibration(0.005f, 150.0f, 1.0f);
                        }

                        if (color_contrast_ < 0.0f)
                            color_contrast_ = 0.0f;

                        if (color_contrast_ > 200.0f)
                            color_contrast_ = 200.0f;

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

                        auto applyContrast = [](float channel, float contrast) -> float {
                            return (channel - 0.5f) * contrast + 0.5f;
                        };

                        r = applyContrast(r, color_contrast_ / 100.0f);
                        g = applyContrast(g, color_contrast_ / 100.0f);
                        b = applyContrast(b, color_contrast_ / 100.0f);

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
}

auto ImGuiOverlayWindow::Destroy() -> void
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplOpenVR_Shutdown();
    ImGui::DestroyContext();
}

auto ImGuiOverlayWindow::SetFrameTime(float refresh_rate) -> void
{
    frame_time_ = 1000.0f / refresh_rate;
    refresh_rate_ = refresh_rate;
}

auto ImGuiOverlayWindow::UpdateDeviceTransform() -> void
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
