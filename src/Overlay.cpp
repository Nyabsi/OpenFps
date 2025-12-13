#include "Overlay.hpp"

#include <backends/imgui_impl_vulkan.h>
#include "backends/imgui_impl_openvr.h"
#include "VulkanRenderer.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <imgui.h>
#include "ImHelper.h"

Overlay::Overlay(const std::string& appKey, const std::string& name, vr::VROverlayType type, int width, int height) : VrOverlay()
{
    surface_ = std::make_unique<Vulkan_Surface>();

    try {
        char overlay_key[100];
        snprintf(overlay_key, 100, "%s-%d", appKey.c_str(), std::rand() % 1024);
        this->Create(type, overlay_key, name.c_str());
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to create overlay.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, "OpenFps", MB_OK);
#endif
        printf("%s\n\n", ex.what());
        std::exit(EXIT_FAILURE);
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    // https://github.com/ocornut/imgui/issues/707#issuecomment-3592676777

    style.Colors[ImGuiCol_WindowBg] = IMGUI_NORMALIZED_RGBA(30, 30, 46, 255);
    style.Colors[ImGuiCol_ChildBg] = IMGUI_NORMALIZED_RGBA(30, 30, 46, 255);
    style.Colors[ImGuiCol_PopupBg] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_Border] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_BorderShadow] = IMGUI_NORMALIZED_RGBA(0, 0, 0, 0);

    style.Colors[ImGuiCol_FrameBg] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_FrameBgHovered] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_FrameBgActive] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);

    style.Colors[ImGuiCol_TitleBg] = IMGUI_NORMALIZED_RGBA(24, 24, 37, 255);
    style.Colors[ImGuiCol_TitleBgActive] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_TitleBgCollapsed] = IMGUI_NORMALIZED_RGBA(24, 24, 37, 255);
    style.Colors[ImGuiCol_MenuBarBg] = IMGUI_NORMALIZED_RGBA(24, 24, 37, 255);

    style.Colors[ImGuiCol_ScrollbarBg] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_ScrollbarGrab] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = IMGUI_NORMALIZED_RGBA(101, 103, 124, 255);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = IMGUI_NORMALIZED_RGBA(147, 153, 178, 255);

    style.Colors[ImGuiCol_CheckMark] = IMGUI_NORMALIZED_RGBA(166, 227, 161, 255);
    style.Colors[ImGuiCol_SliderGrab] = IMGUI_NORMALIZED_RGBA(116, 199, 236, 255);
    style.Colors[ImGuiCol_SliderGrabActive] = IMGUI_NORMALIZED_RGBA(137, 180, 250, 255);

    style.Colors[ImGuiCol_Button] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_ButtonHovered] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_ButtonActive] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);

    style.Colors[ImGuiCol_Header] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_HeaderHovered] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_HeaderActive] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);

    style.Colors[ImGuiCol_Separator] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_SeparatorHovered] = IMGUI_NORMALIZED_RGBA(203, 166, 247, 255);
    style.Colors[ImGuiCol_SeparatorActive] = IMGUI_NORMALIZED_RGBA(203, 166, 247, 255);

    style.Colors[ImGuiCol_ResizeGrip] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);
    style.Colors[ImGuiCol_ResizeGripHovered] = IMGUI_NORMALIZED_RGBA(203, 166, 247, 255);
    style.Colors[ImGuiCol_ResizeGripActive] = IMGUI_NORMALIZED_RGBA(203, 166, 247, 255);

    style.Colors[ImGuiCol_Tab] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_TabHovered] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);
    style.Colors[ImGuiCol_TabActive] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_TabUnfocused] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_TabUnfocusedActive] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);

    style.Colors[ImGuiCol_PlotLines] = IMGUI_NORMALIZED_RGBA(137, 180, 250, 255);
    style.Colors[ImGuiCol_PlotLinesHovered] = IMGUI_NORMALIZED_RGBA(250, 179, 135, 255);
    style.Colors[ImGuiCol_PlotHistogram] = IMGUI_NORMALIZED_RGBA(148, 226, 213, 255);
    style.Colors[ImGuiCol_PlotHistogramHovered] = IMGUI_NORMALIZED_RGBA(166, 227, 161, 255);

    style.Colors[ImGuiCol_TableHeaderBg] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_TableBorderStrong] = IMGUI_NORMALIZED_RGBA(63, 64, 86, 255);
    style.Colors[ImGuiCol_TableBorderLight] = IMGUI_NORMALIZED_RGBA(49, 50, 68, 255);
    style.Colors[ImGuiCol_TableRowBg] = IMGUI_NORMALIZED_RGBA(0, 0, 0, 0);
    style.Colors[ImGuiCol_TableRowBgAlt] = IMGUI_NORMALIZED_RGBA(255, 255, 255, 15);

    style.Colors[ImGuiCol_TextSelectedBg] = IMGUI_NORMALIZED_RGBA(74, 77, 99, 255);
    style.Colors[ImGuiCol_DragDropTarget] = IMGUI_NORMALIZED_RGBA(249, 226, 175, 255);
    style.Colors[ImGuiCol_NavHighlight] = IMGUI_NORMALIZED_RGBA(180, 190, 254, 255);
    style.Colors[ImGuiCol_NavWindowingHighlight] = IMGUI_NORMALIZED_RGBA(255, 255, 255, 178);
    style.Colors[ImGuiCol_NavWindowingDimBg] = IMGUI_NORMALIZED_RGBA(204, 204, 204, 51);
    style.Colors[ImGuiCol_ModalWindowDimBg] = IMGUI_NORMALIZED_RGBA(0, 0, 0, 89);

    style.Colors[ImGuiCol_Text] = IMGUI_NORMALIZED_RGBA(205, 214, 244, 255);
    style.Colors[ImGuiCol_TextDisabled] = IMGUI_NORMALIZED_RGBA(163, 168, 195, 255);

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.ScaleAllSizes(1.0f);
    style.FontScaleDpi = 1.0f;

    style.Alpha = 0.5f;

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
        .handle = this->Handle(),
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
        .Instance = g_vulkanRenderer->Instance(),
        .PhysicalDevice = g_vulkanRenderer->PhysicalDevice(),
        .Device = g_vulkanRenderer->Device(),
        .QueueFamily = g_vulkanRenderer->QueueFamily(),
        .Queue = g_vulkanRenderer->Queue(),
        .DescriptorPool = g_vulkanRenderer->DescriptorPool(),
        .RenderPass = VK_NULL_HANDLE,
        .MinImageCount = 16,
        .ImageCount = 16,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache = g_vulkanRenderer->PipelineCache(),
        .Subpass = 0,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = pipeline_rendering_create_info,
        .Allocator = g_vulkanRenderer->Allocator(),
        .CheckVkResultFn = nullptr,
    };

    ImGui_ImplVulkan_Init(&init_info);

    g_vulkanRenderer->SetupSurface(this, width, height, surface_format);
}

Overlay::~Overlay()
{
    this->Destroy();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplOpenVR_Shutdown();

    ImGui::DestroyContext();
}

auto Overlay::Render() -> void
{
    assert(false && "Did you forget to override \'Render\' for Overlay?!");
}

auto Overlay::Update() -> void
{
    assert(false && "Did you forget to override \'Update\' for Overlay?!");
}

auto Overlay::Draw() -> void
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    g_vulkanRenderer->RenderSurface(draw_data, this);
}