#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sstream>
#include <fstream>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <openvr.h>

#include "VulkanRenderer.h"
#include "VulkanUtils.h"

#include "PerformanceOverlay.h"

#include "VrOverlay.h"
#include "VrUtils.h"

#include "backends/imgui_impl_openvr.h"

#ifdef _WIN32
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

static VulkanRenderer* g_vulkanRenderer = new VulkanRenderer();
static PerformanceOverlay* g_performanceOverlay = new PerformanceOverlay();
static VrOverlay* g_overlay = new VrOverlay();

static uint64_t g_last_frame_time = SDL_GetTicksNS();
static float g_hmd_refresh_rate = 24.0f;
static bool g_ticking = true;
static bool g_keyboard_global_show = false;
static float g_overlay_width = -1.0f;
static uint32_t last_index = vr::k_unTrackedDeviceIndexInvalid;

static vr::ETrackedControllerRole g_overlay_handedness = vr::TrackedControllerRole_Invalid;
static glm::vec3 g_position = {};
static glm::quat g_rotation = {};

#define APP_KEY     "Nyabsi.OpenFps"
#define APP_NAME    "OpenFps"

#define WIN_WIDTH   500
#define WIN_HEIGHT  370

static auto UpdateApplicationRefreshRate() -> void
{
    try {
        auto hmd_properties = VrTrackedDeviceProperties::FromDeviceIndex(vr::k_unTrackedDeviceIndex_Hmd);
        hmd_properties.CheckConnection();
        g_hmd_refresh_rate = hmd_properties.GetFloat(vr::Prop_DisplayFrequency_Float);
        g_performanceOverlay->SetFrameTime(g_hmd_refresh_rate);
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to update HMD Refresh Rate\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        if (g_hmd_refresh_rate == 24.0f)
            std::exit(EXIT_FAILURE);
    }
}

int main(
    [[maybe_unused]] int argc, 
    [[maybe_unused]] char** argv
) {
#ifdef _WIN32
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
    std::srand(std::time(nullptr));

    try {
        OpenVRInit(vr::VRApplication_Background);
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize OpenVR.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return EXIT_FAILURE;
    }

    UpdateApplicationRefreshRate();

    try {
        char overlay_key[100];
        snprintf(overlay_key, 100, "%s-%d", APP_KEY, std::rand() % 1024);

        g_overlay->Create(vr::VROverlayType_World, overlay_key, APP_NAME);

        g_overlay->SetInputMethod(vr::VROverlayInputMethod_Mouse);

        g_overlay->EnableFlag(vr::VROverlayFlags_SendVRDiscreteScrollEvents);
        g_overlay->EnableFlag(vr::VROverlayFlags_EnableClickStabilization);

        g_overlay->SetWidth(0.15);
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize Overlay.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return EXIT_FAILURE;
    }

    g_vulkanRenderer->Initialize();
    g_performanceOverlay->Initialize(g_vulkanRenderer, g_overlay, WIN_WIDTH, WIN_HEIGHT);

    try {
        if (!OpenVRManifestInstalled(APP_KEY)) OpenVRManifestInstall();
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize OpenVR Manifest.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return EXIT_FAILURE;
    }

    if (!vr::VRApplications()->GetApplicationAutoLaunch(APP_KEY))
    {
        vr::VRApplications()->SetApplicationAutoLaunch(APP_KEY, false);
        vr::VRApplications()->SetApplicationAutoLaunch(APP_KEY, true);
    }

    // allow our application to override some settings.
    vr::VRSettings()->SetInt32(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleManualOverride_Bool, true);
    vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float, 1.0f);
    vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float, 1.0f);
    vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float, 1.0f);

    SDL_Event event = {};
    vr::VREvent_t vr_event = {};

    while (g_ticking)
    {
        while (vr::VROverlay()->PollNextOverlayEvent(g_overlay->Handle(), &vr_event, sizeof(vr_event))) 
        {
            ImGui_ImplOpenVR_ProcessOverlayEvent(vr_event);

            switch (vr_event.eventType) 
            {
                case vr::VREvent_PropertyChanged:
                {
                    if (vr_event.data.property.prop == vr::Prop_DisplayFrequency_Float) {
                        UpdateApplicationRefreshRate();
                    }
                    break;
                }
                case vr::VREvent_KeyboardOpened_Global:
                    if (vr_event.data.keyboard.overlayHandle != g_overlay->Handle())
                        g_keyboard_global_show = true;
                    break;
                case vr::VREvent_KeyboardClosed_Global:
                    if (vr_event.data.keyboard.overlayHandle != g_overlay->Handle())
                        g_keyboard_global_show = false;
                    break;
                case vr::VREvent_Quit:
                {
                    vr::VRSystem()->AcknowledgeQuit_Exiting();
                    g_ticking = false;
                    return false;
                }
            }
        }

        if (g_overlay->IsVisible() && !vr::VROverlay()->IsDashboardVisible() && g_performanceOverlay->DisplayMode() == Overlay_DisplayMode_Dashboard)
            g_overlay->Hide();

        if (g_overlay->IsVisible() && vr::VROverlay()->IsDashboardVisible() && g_keyboard_global_show)
            g_overlay->Hide();

        if (!g_overlay->IsVisible() && (g_performanceOverlay->DisplayMode() == Overlay_DisplayMode_Always || (g_performanceOverlay->DisplayMode() == Overlay_DisplayMode_Dashboard && vr::VROverlay()->IsDashboardVisible())) && !g_keyboard_global_show)
            g_overlay->Show();

        const auto handedness = static_cast<vr::ETrackedControllerRole>(g_performanceOverlay->Handedness());
        const auto scale = g_performanceOverlay->OverlayScale();

        // Device relative offset
        const auto transform = g_performanceOverlay->Transform();
        glm::vec3 position = transform.position;
        glm::quat rotation = transform.rotation;

        if (g_overlay_width != scale) {
            g_overlay->SetWidth(scale);
            g_overlay_width = scale;
        }

        auto hand_index = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(handedness);
        if (g_overlay_handedness != handedness || (g_position != position && g_rotation != rotation) || last_index == vr::k_unTrackedDeviceIndexInvalid && hand_index != vr::k_unTrackedDeviceIndexInvalid) {
            g_overlay->SetTransformDeviceRelative(handedness, position, rotation);
            g_overlay_handedness = handedness;
            g_position = position;
            g_rotation = rotation;

            if (last_index == vr::k_unTrackedDeviceIndexInvalid)
                last_index = hand_index;
        }

        g_performanceOverlay->Update();
        g_performanceOverlay->Draw();

        if ((g_performanceOverlay->DisplayMode() == Overlay_DisplayMode_Always || (g_performanceOverlay->DisplayMode() == Overlay_DisplayMode_Dashboard && vr::VROverlay()->IsDashboardVisible())) && !g_keyboard_global_show) {
            ImDrawData* draw_data = ImGui::GetDrawData();
            g_vulkanRenderer->RenderSurface(draw_data, g_overlay);
        }

        uint64_t target_time = static_cast<uint64_t>((static_cast<float>(1000000000) / g_hmd_refresh_rate));
        const uint64_t frame_duration = (SDL_GetTicksNS() - g_last_frame_time);

        if (frame_duration < target_time) {
            vr::VROverlay()->WaitFrameSync((target_time - frame_duration) * 1000000000);
            SDL_DelayPrecise(target_time - frame_duration);
        }

        g_last_frame_time = SDL_GetTicksNS();
    }

    VkResult vk_result = vkDeviceWaitIdle(g_vulkanRenderer->Device());
    VK_VALIDATE_RESULT(vk_result);

    g_performanceOverlay->Destroy();
    g_vulkanRenderer->Destroy();

    SDL_Quit();

    return 0;
}
