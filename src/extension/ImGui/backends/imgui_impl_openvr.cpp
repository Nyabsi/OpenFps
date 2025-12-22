// dear imgui: Platform backend for OpenVR
// This needs to be used along with a Renderer Backend (e.g. Vulkan)

// Implemented features:
//  [X] Platform: Virtual keyboard support
//  [X] Platform: Mouse emulation
//  [X] Platform: Laser input

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include <imgui.h>
#ifndef IMGUI_DISABLE
#include "imgui_impl_openvr.h"

#include <openvr.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

struct ImGui_ImplOpenVR_Data {
    uintptr_t handle;
    uint32_t width;
    uint32_t height;
    bool keyboard_active;
#ifdef _WIN32
    uint64_t ticks_per_second;
#endif
    uint64_t time;
    vr::HmdVector2_t mouse_scale;
    bool laser_hit_dirty;
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    ImGui_ImplOpenVR_Data() { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplOpenVR_Data* ImGui_ImplOpenVR_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplOpenVR_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

bool ImGui_ImplOpenVR_Init(ImGui_ImplOpenVR_InitInfo* initInfo)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    // Setup backend capabilities flags
    ImGui_ImplOpenVR_Data* bd = IM_NEW(ImGui_ImplOpenVR_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_openvr";

    bd->handle = initInfo->handle;
    bd->width = initInfo->width;
    bd->height = initInfo->height;

    bd->keyboard_active = false;
    bd->laser_hit_dirty = false;

#ifdef _WIN32
    uint64_t perf_frequency = {};
    if (!QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency))
        return false;

    uint64_t current_time = {};
    if (!QueryPerformanceCounter((LARGE_INTEGER*)&current_time))
        return false;

    bd->ticks_per_second = perf_frequency;
    bd->time = current_time;
#endif
    return true;
}

bool ImGui_ImplOpenVR_ProcessOverlayEvent(const vr::VREvent_t& event)
{
    ImGui_ImplOpenVR_Data* bd = ImGui_ImplOpenVR_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenVR_Init()?");

    ImGuiIO& io = ImGui::GetIO();
    switch (event.eventType)
    {
        case vr::VREvent_MouseMove:
        {
            // OpenGL uses coordinate space Bottom Left == 0,0 where as Vulkan is Top Left == 0,0
            // So we need to flip the y-axis to get the correct mouse position data
            io.AddMousePosEvent(event.data.mouse.x, io.DisplaySize.y - event.data.mouse.y);
            break;
        }
        case vr::VREvent_MouseButtonDown:
        {
            uint64_t mask = event.data.mouse.button;
            mask &= vr::VRMouseButton_Left | vr::VRMouseButton_Right | vr::VRMouseButton_Middle;
            // Ensure that the event sends a bitmask with only a single flag
            // this may be redundant but is here in case driver sends bad data
            if (mask && (mask & (mask - 1))) {
                break;
            }

            int mouse_button = ImGuiMouseButton_COUNT;

            // Most drivers only send VRMouseButton_Left but there are some drivers
            // which also send VRMouseButton_Right and VRMouseButton_Middle
            // right click is often mapped to A, X or B
            // middle click is often mapped to trackpad click
            if (event.data.mouse.button & vr::VRMouseButton_Left)
                mouse_button = ImGuiMouseButton_Left;
            else if (event.data.mouse.button & vr::VRMouseButton_Right)
                mouse_button = ImGuiMouseButton_Right;
            else if (event.data.mouse.button & vr::VRMouseButton_Middle)
                mouse_button = ImGuiMouseButton_Middle;

            if (mouse_button < ImGuiMouseButton_COUNT)
                io.AddMouseButtonEvent(mouse_button, true);
            break;
        }
        case vr::VREvent_MouseButtonUp:
        {
            uint64_t mask = event.data.mouse.button;
            mask &= vr::VRMouseButton_Left | vr::VRMouseButton_Right | vr::VRMouseButton_Middle;
            if (mask && (mask & (mask - 1))) {
                break;
            }

            int mouse_button = ImGuiMouseButton_COUNT;

            if (event.data.mouse.button & vr::VRMouseButton_Left)
                mouse_button = ImGuiMouseButton_Left;
            else if (event.data.mouse.button & vr::VRMouseButton_Right)
                mouse_button = ImGuiMouseButton_Right;
            else if (event.data.mouse.button & vr::VRMouseButton_Middle)
                mouse_button = ImGuiMouseButton_Middle;

            if (mouse_button < ImGuiMouseButton_COUNT)
                io.AddMouseButtonEvent(mouse_button, false);
            break;
        }
        case vr::VREvent_ScrollDiscrete:
        case vr::VREvent_ScrollSmooth:
        {
            // Emulate physical mouse behaviour by only sending y-axis
            // VREvent_ScrollDiscrete sends discrete values [-1.0, 1.0]
            // VREvent_ScrollSmooth sends continuous values [-1.0, 1.0]
            const float y = event.data.scroll.ydelta;
            if (y != 0.0f)
                io.AddMouseWheelEvent(0.0f, y);
            break;
        }
        case vr::VREvent_KeyboardCharInput:
        {
            // Some special inputs ie. Backspace, Enter, etc...
            // are not handled by AddInputCharactersUTF8
            // because it only allows UTF-8 character input
            switch (event.data.keyboard.cNewInput[0])
            {
                case 8: // Backspace
                {
                    io.AddKeyEvent(ImGuiKey_Backspace, true);
                    io.AddKeyEvent(ImGuiKey_Backspace, false);
                    break;
                }
                case 10: // Enter
                {
                    io.AddKeyEvent(ImGuiKey_Enter, true);
                    io.AddKeyEvent(ImGuiKey_Enter, false);
                    vr::VROverlay()->HideKeyboard();
                    break;
                }
                case 27: // arrow keys
                {
                    uint8_t direction = event.data.keyboard.cNewInput[2];
                    switch (direction)
                    {
                    case 68:
                    {
                        io.AddKeyEvent(ImGuiKey_LeftArrow, true);
                        io.AddKeyEvent(ImGuiKey_LeftArrow, false);
                        break;
                    }
                    case 67:
                    {
                        io.AddKeyEvent(ImGuiKey_RightArrow, true);
                        io.AddKeyEvent(ImGuiKey_RightArrow, false);
                        break;
                    }
                    case 65:
                    {
                        io.AddKeyEvent(ImGuiKey_UpArrow, true);
                        io.AddKeyEvent(ImGuiKey_UpArrow, false);
                        break;
                    }
                    case 66:
                    {
                        io.AddKeyEvent(ImGuiKey_DownArrow, true);
                        io.AddKeyEvent(ImGuiKey_DownArrow, false);
                        break;
                    }
                    }
                    break;
                }
                default:
                {
                    io.AddInputCharactersUTF8(event.data.keyboard.cNewInput);
                    break;
                }
            }
            break;
        }
        case vr::VREvent_KeyboardClosed_Global:
        {
            if (!vr::VROverlay())
                return false;

            // let's check when VREvent_KeyboardClosed_Global is sent is our keyboard is still shown
            // this may happen because the dashboard or overlay was closed when the keyboard was open
            if (event.data.keyboard.overlayHandle == bd->handle && bd->keyboard_active) {
                io.AddKeyEvent(ImGuiKey_Enter, true);
                io.AddKeyEvent(ImGuiKey_Enter, false);
                vr::VROverlay()->HideKeyboard();
            }
            break;
        }
    }

    return true;
}

bool ImGui_ImplOpenVR_ProcessLaserInput(vr::ETrackedControllerRole role)
{
    ImGui_ImplOpenVR_Data* bd = ImGui_ImplOpenVR_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized!");

    // Reset before ImGui_ImplOpenVR_NewFrame is called, because that means we got new Input before we should actually clear the mouse position.
    bd->laser_hit_dirty = false;

    ImGuiIO& io = ImGui::GetIO();

    vr::TrackedDeviceIndex_t controller = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(role);
    if (controller == vr::k_unTrackedDeviceIndexInvalid)
        return false;

    vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, 0.0f, bd->poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t& pose = bd->poses[controller];
    if (!pose.bPoseIsValid)
        return false;

    vr::VRControllerState_t controllerState = {};
    vr::VRSystem()->GetControllerState(controller, &controllerState, sizeof(controllerState));

    vr::HmdMatrix34_t finalMatrix = pose.mDeviceToAbsoluteTracking;

    static char renderModelName[vr::k_unMaxPropertyStringSize] = {};
    vr::VRSystem()->GetStringTrackedDeviceProperty(
        controller,
        vr::Prop_RenderModelName_String,
        renderModelName,
        sizeof(renderModelName)
    );

    vr::RenderModel_ControllerMode_State_t controllerModeState = {};
    vr::RenderModel_ComponentState_t componentState = {};

    if (vr::VRRenderModels()->GetComponentState(renderModelName, "tip", &controllerState, &controllerModeState, &componentState)) {
        vr::HmdMatrix34_t result = {};

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m[i][j] = 0.0f;
                for (int k = 0; k < 3; k++)
                    result.m[i][j] +=
                    finalMatrix.m[i][k] *
                    componentState.mTrackingToComponentRenderModel.m[k][j];

                if (j == 3)
                    result.m[i][j] += finalMatrix.m[i][3];
            }
        }

        finalMatrix = result;
    }

    vr::HmdVector3_t origin = {
        finalMatrix.m[0][3],
        finalMatrix.m[1][3],
        finalMatrix.m[2][3]
    };

    vr::HmdVector3_t forward = {
        -finalMatrix.m[0][2],
        -finalMatrix.m[1][2],
        -finalMatrix.m[2][2]
    };

    vr::HmdVector3_t upward = {
         finalMatrix.m[0][1],
         finalMatrix.m[1][1],
         finalMatrix.m[2][1]
    };

    constexpr float tilt_offset = 60.0f * (3.14159265358979323846f / 180.0f);
    vr::HmdVector3_t direction = {
        forward.v[0] * cosf(tilt_offset) - upward.v[0] * sinf(tilt_offset),
        forward.v[1] * cosf(tilt_offset) - upward.v[1] * sinf(tilt_offset),
        forward.v[2] * cosf(tilt_offset) - upward.v[2] * sinf(tilt_offset)
    };

    float len = sqrtf(direction.v[0] * direction.v[0] + direction.v[1] * direction.v[1] + direction.v[2] * direction.v[2]);
    direction.v[0] /= len;
    direction.v[1] /= len;
    direction.v[2] /= len;

    constexpr float tip_offset = 0.05f;
    origin.v[0] += direction.v[0] * tip_offset;
    origin.v[1] += direction.v[1] * tip_offset;
    origin.v[2] += direction.v[2] * tip_offset;

    vr::VROverlayIntersectionParams_t params = {};
    params.eOrigin = vr::TrackingUniverseSeated;
    params.vSource = origin;
    params.vDirection = direction;

    vr::VROverlayIntersectionResults_t results = {};

    if (!vr::VROverlay()->ComputeOverlayIntersection(bd->handle, &params, &results)) {
        bd->laser_hit_dirty = true;
        return false;
    }

    // convert UV -> ImGui screen space
    const float x = results.vUVs.v[0] * io.DisplaySize.x;
    const float y = (1.0f - results.vUVs.v[1]) * io.DisplaySize.y;

    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, controllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
    return true;
}

void ImGui_ImplOpenVR_Shutdown()
{
    ImGui_ImplOpenVR_Data* bd = ImGui_ImplOpenVR_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenVR_Init()?");

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    IM_DELETE(bd);
}


static float ImGui_ImplOpenVR_GetDeltaTime()
{
    ImGui_ImplOpenVR_Data* bd = ImGui_ImplOpenVR_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenVR_Init()?");

#ifdef _WIN32
    uint64_t current_time = {};
    QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    float deltaTime = (float)(current_time - bd->time) / bd->ticks_per_second;
    bd->time = current_time;
    return deltaTime;
#else
    struct timespec current_timespec;
    clock_gettime(CLOCK_MONOTONIC, &current_timespec);
    double current_time = (double)(current_timespec.tv_sec) + (current_timespec.tv_nsec / 1000000000.0);
    float deltaTime = bd->time > 0.0 ? (float)(current_time - bd->time) : (float)(1.0f / 60.0f);
    bd->time = current_time;
    return deltaTime;
#endif
}

void ImGui_ImplOpenVR_NewFrame()
{
    ImGui_ImplOpenVR_Data* bd = ImGui_ImplOpenVR_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplOpenVR_Init()?");

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantTextInput) {
        bd->keyboard_active = false;
    }

    if (vr::VROverlay()->IsOverlayVisible(bd->handle) && !bd->keyboard_active && io.WantTextInput) {
        vr::VROverlay()->ShowKeyboardForOverlay(bd->handle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Minimal | vr::KeyboardFlag_HideDoneKey | vr::KeyboardFlag_ShowArrowKeys, "ImGui OpenVR Virtual Keyboard", 1, "", NULL);
        bd->keyboard_active = true;
    }

    io.DisplaySize = ImVec2(bd->width, bd->height);
    if (bd->width > 0 && bd->height > 0) {
        io.DisplayFramebufferScale = ImVec2(1, 1);
        bd->mouse_scale = { io.DisplaySize.x, io.DisplaySize.y };
        vr::VROverlay()->SetOverlayMouseScale(bd->handle, &bd->mouse_scale);

        vr::VRTextureBounds_t bounds;
        bounds.uMin = 0.0f;
        bounds.uMax = 1.0f;
        bounds.vMin = 0.0f;
        bounds.vMax = 1.0f;
        vr::VROverlay()->SetOverlayTextureBounds(bd->handle, &bounds);
    }

    if (bd->laser_hit_dirty) {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        bd->laser_hit_dirty = false;
    }

    io.DeltaTime = ImGui_ImplOpenVR_GetDeltaTime();
}

#endif // #ifndef IMGUI_DISABLE