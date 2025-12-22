#include "DashboardOverlay.h"

#include <algorithm>
#include <chrono>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <extension/ImGui/backends/imgui_impl_openvr.h>

#include <config.hpp>

#define OVERLAY_KEY     "Nyabsi.OpenFps"
#define OVERLAY_NAME    "OpenFps Dashboard"
#define OVERLAY_WIDTH   1600
#define OVERLAY_HEIGHT  900

struct CachedProcessRow
{
    uint32_t    pid;
    ProcessInfo info;
    GpuInfo     gpu;
};

static std::vector<CachedProcessRow> g_cached_rows;
static bool g_rows_dirty = true;
static ImGuiTableSortSpecs g_cached_sort = {};
static std::chrono::steady_clock::time_point g_last_update;


DashboardOverlay::DashboardOverlay() : Overlay(OVERLAY_KEY, OVERLAY_NAME, vr::VROverlayType_Dashboard, OVERLAY_WIDTH, OVERLAY_HEIGHT)
{
    try {
        std::string thumbnail_path = {};
        thumbnail_path += SDL_GetCurrentDirectory();
        thumbnail_path += "icon.png";
        this->SetThumbnail(thumbnail_path);

        this->SetInputMethod(vr::VROverlayInputMethod_Mouse);
        this->SetWidth(3.0f);

        this->EnableFlag(vr::VROverlayFlags_SendVRDiscreteScrollEvents);
        this->EnableFlag(vr::VROverlayFlags_EnableClickStabilization);
    }
    catch (const std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize the overlay.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        std::exit(EXIT_FAILURE);
    }

    task_monitor_.Initialize();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(2.0f);
    style.FontScaleDpi = 2.0f;

    g_last_update = std::chrono::steady_clock::now();
}

auto DashboardOverlay::Render()-> bool
{
    if (!Overlay::Render())
        return false;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplOpenVR_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos(io.DisplaySize.x, io.DisplaySize.y);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(pos, ImGuiCond_Always);

    ImGui::Begin("OpenFps", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove);

    ImGui::BeginChild("process_list_scroller", ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Hideable |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Sortable;

    if (ImGui::BeginTable("process_list", 9, flags))
    {
        ImGui::TableSetupColumn("PID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("CPU %");
        ImGui::TableSetupColumn("GPU %");
        ImGui::TableSetupColumn("Video %");
        ImGui::TableSetupColumn("D-VRAM");
        ImGui::TableSetupColumn("S-VRAM");
        ImGui::TableSetupColumn("RAM");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();

        bool sort_changed =
            sort_specs &&
            (g_cached_sort.SpecsCount != sort_specs->SpecsCount ||
                memcmp(&g_cached_sort, sort_specs, sizeof(ImGuiTableSortSpecs)) != 0);

        if (g_rows_dirty || sort_changed)
        {
            g_cached_rows.clear();
            g_cached_rows.reserve(task_monitor_.Processes().size());

            for (auto& [pid, info] : task_monitor_.Processes())
            {
                g_cached_rows.push_back({
                    pid,
                    info,
                    getCurrentlyUsedGpu(info)
                    });
            }

            if (sort_specs && sort_specs->SpecsCount > 0)
            {
                const auto& s = sort_specs->Specs[0];

                std::sort(g_cached_rows.begin(), g_cached_rows.end(),
                    [&](const CachedProcessRow& a,
                        const CachedProcessRow& b)
                    {
                        switch (s.ColumnIndex)
                        {
                        case 0: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.pid < b.pid : a.pid > b.pid;

                        case 1: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.info.process_name < b.info.process_name
                            : a.info.process_name > b.info.process_name;

                        case 2: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.info.cpu.total_cpu_usage < b.info.cpu.total_cpu_usage
                            : a.info.cpu.total_cpu_usage > b.info.cpu.total_cpu_usage;

                        case 3: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? gpuPercentage(a.gpu) < gpuPercentage(b.gpu)
                            : gpuPercentage(a.gpu) > gpuPercentage(b.gpu);

                        case 4: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? gpuVideoPercentage(a.gpu) < gpuVideoPercentage(b.gpu)
                            : gpuVideoPercentage(a.gpu) > gpuVideoPercentage(b.gpu);

                        case 5: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.gpu.memory.dedicated_vram_usage <
                            b.gpu.memory.dedicated_vram_usage
                            : a.gpu.memory.dedicated_vram_usage >
                            b.gpu.memory.dedicated_vram_usage;

                        case 6: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.gpu.memory.shared_vram_usage <
                            b.gpu.memory.shared_vram_usage
                            : a.gpu.memory.shared_vram_usage >
                            b.gpu.memory.shared_vram_usage;

                        case 7: return s.SortDirection == ImGuiSortDirection_Ascending
                            ? a.info.memory_usage <
                            b.info.memory_usage
                            : a.info.memory_usage >
                            b.info.memory_usage;
                        }
                        return false;
                    });
            }

            if (sort_specs)
                g_cached_sort = *sort_specs;

            g_rows_dirty = false;
        }

        for (auto& row : g_cached_rows)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%llu", row.pid);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", row.info.process_name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f %%", row.info.cpu.total_cpu_usage);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f %%", gpuPercentage(row.gpu));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.1f %%", gpuVideoPercentage(row.gpu));

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.0f MB",
                row.gpu.memory.dedicated_vram_usage / (1000.0f * 1000.0f));

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.0f MB",
                row.gpu.memory.shared_vram_usage / (1000.0f * 1000.0f));

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.0f MB",
                row.info.memory_usage / (1024.0f * 1024.0f));

            ImGui::TableSetColumnIndex(8);
            ImGui::PushID(row.pid);
            if (ImGui::Button("Kill"))
            {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, row.pid);
                if (process)
                {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::Render();

    return true;
}

auto DashboardOverlay::Update() -> void
{
    Overlay::Update();

    constexpr auto UPDATE_INTERVAL = std::chrono::milliseconds(500);
    auto now = std::chrono::steady_clock::now();

    if (now - g_last_update > UPDATE_INTERVAL)
    {
        task_monitor_.Update();
        g_rows_dirty = true;
        g_last_update = now;
    }
}


auto DashboardOverlay::Destroy() -> void
{
    task_monitor_.Destroy();
}