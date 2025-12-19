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

#include "HandOverlay.h"
#include "DashboardOverlay.h"

#include "VrOverlay.h"
#include "VrUtils.h"

#include "backends/imgui_impl_openvr.h"

#ifdef _WIN32
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

VulkanRenderer* g_vulkanRenderer = new VulkanRenderer();

static HandOverlay* g_performanceOverlay;
static DashboardOverlay* g_ProcessList;

static uint64_t g_last_frame_time = SDL_GetTicksNS();
static float g_hmd_refresh_rate = 24.0f;
static bool g_ticking = true;

#define APP_KEY     "Nyabsi.OpenFps"
#define APP_NAME    "OpenFps"

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


    try {
        g_vulkanRenderer->Initialize();
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize Vulkan.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return EXIT_FAILURE;
    }

    g_performanceOverlay = new HandOverlay();
    g_ProcessList = new DashboardOverlay();

    UpdateApplicationRefreshRate();

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
        while (vr::VRSystem()->PollNextEvent(&vr_event, sizeof(vr_event)))
        {
            switch (vr_event.eventType) 
            {
                case vr::VREvent_PropertyChanged:
                {
                    if (vr_event.data.property.prop == vr::Prop_DisplayFrequency_Float) {
                        UpdateApplicationRefreshRate();
                    }
                    break;
                }
                case vr::VREvent_Quit:
                {
                    vr::VRSystem()->AcknowledgeQuit_Exiting();
                    g_ticking = false;
                    return false;
                }
            }
        }
        
        g_performanceOverlay->Update();
        if (g_performanceOverlay->Render())
            g_performanceOverlay->Draw();

        g_ProcessList->Update();
        if (g_ProcessList->Render())
            g_ProcessList->Draw();

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
    g_ProcessList->Destroy();

    g_vulkanRenderer->DestroySurface(g_performanceOverlay->Surface());
    g_vulkanRenderer->DestroySurface(g_ProcessList->Surface());
    g_vulkanRenderer->Destroy();

    SDL_Quit();

    return 0;
}
