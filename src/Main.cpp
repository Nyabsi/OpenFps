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

#include <config.hpp>

#include <renderer/VulkanRenderer.h>
#include <helper/VulkanHelper.h>

#include <overlay/controller/ControllerOverlay.h>
#include <overlay/dashboard/DashboardOverlay.h>

#include <extension/OpenVR/VrUtils.h>

#ifdef _WIN32
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

VulkanRenderer* g_vulkanRenderer = new VulkanRenderer();

std::unique_ptr<ControllerOverlay> g_processInformation;
std::unique_ptr<DashboardOverlay>  g_ProcessList;

static uint64_t g_last_frame_time = SDL_GetTicksNS();
static float g_hmd_refresh_rate = 24.0f;
static bool g_ticking = true;

static auto UpdateApplicationRefreshRate() -> void
{
    try {
        auto hmd_properties = VrTrackedDeviceProperties::FromDeviceIndex(vr::k_unTrackedDeviceIndex_Hmd);
        hmd_properties.CheckConnection();
        g_hmd_refresh_rate = hmd_properties.GetFloat(vr::Prop_DisplayFrequency_Float);
        g_processInformation->SetFrameTime(g_hmd_refresh_rate);
    }
    catch (const std::exception& ex) {
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
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    try {
        OpenVRInit(vr::VRApplication_Background);

        if (!OpenVRManifestInstalled(APP_KEY)) {
            OpenVRManifestInstall();
        }

        if (!vr::VRApplications()->GetApplicationAutoLaunch(APP_KEY))
        {
            vr::VRApplications()->SetApplicationAutoLaunch(APP_KEY, false);
            vr::VRApplications()->SetApplicationAutoLaunch(APP_KEY, true);
        }

        // TODO: this resets it each time, is this something we want to really do?
        vr::VRSettings()->SetInt32(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleManualOverride_Bool, true);
        vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainR_Float, 1.0f);
        vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainG_Float, 1.0f);
        vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_HmdDisplayColorGainB_Float, 1.0f);

        g_vulkanRenderer->Initialize();
    }
    catch (const std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return EXIT_FAILURE;
    }

    g_processInformation = std::make_unique<ControllerOverlay>();
    g_ProcessList = std::make_unique<DashboardOverlay>();

    UpdateApplicationRefreshRate();

    for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
        if (i == vr::k_unTrackedDeviceIndex_Hmd)
            continue;
        g_processInformation->AddMonitoredDeviceById(i);
    }

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
                    if (vr_event.data.property.prop == vr::Prop_DeviceBatteryPercentage_Float) {
                        g_processInformation->UpdateBatteryPercentageForDeviceById(vr_event.trackedDeviceIndex);
                    }
                    break;
                }
                case vr::VREvent_TrackedDeviceActivated:
                {
                    g_processInformation->AddMonitoredDeviceById(vr_event.trackedDeviceIndex);
                    break;
                }
                case vr::VREvent_TrackedDeviceDeactivated:
                {
                    g_processInformation->RemoveMonitoredDeviceById(vr_event.trackedDeviceIndex);
                    break;
                }
                case vr::VREvent_Quit:
                {
                    vr::VRSystem()->AcknowledgeQuit_Exiting();
                    g_ticking = false;
                    break;
                }
            }
        }
        
        g_processInformation->Update();
        if (g_processInformation->Render())
            g_processInformation->Draw();

        g_ProcessList->Update();
        if (g_ProcessList->Render())
            g_ProcessList->Draw();

        const uint64_t target_time_ns = static_cast<uint64_t>(1'000'000'000.0 / g_hmd_refresh_rate);
        const uint64_t frame_duration_ns = SDL_GetTicksNS() - g_last_frame_time;

        if (frame_duration_ns < target_time_ns)
        {
            const uint32_t timeout_ms = static_cast<uint32_t>((target_time_ns - frame_duration_ns) / 1'000'000);
            vr::VROverlay()->WaitFrameSync(timeout_ms);

            const uint64_t remaining_ns = target_time_ns - (SDL_GetTicksNS() - g_last_frame_time);
            if (remaining_ns > 0)
                SDL_DelayPrecise(remaining_ns);
        }

        g_last_frame_time = SDL_GetTicksNS();
    }

    VkResult vk_result = vkDeviceWaitIdle(g_vulkanRenderer->Device());
    VK_VALIDATE_RESULT(vk_result);

    g_processInformation->Destroy();
    g_ProcessList->Destroy();

    g_vulkanRenderer->DestroySurface(g_processInformation->Surface());
    g_vulkanRenderer->DestroySurface(g_ProcessList->Surface());
    g_vulkanRenderer->Destroy();

    SDL_Quit();

    return 0;
}
