#pragma once

#include <Windows.h>
#include <pdh.h>
#include <string>
#include <unordered_map>
#include <stdint.h>
#include <dxgi1_6.h>
#include <ranges>

struct GpuEngine {
    uint32_t engine_index;
	std::string engine_type;
    float utilization_percentage;
};

struct VRAMInfo {
    size_t dedicated_vram_usage;
    size_t shared_vram_usage;
    size_t dedicated_available;
    size_t shared_available;
};

struct GpuInfo {
    struct {
        uint32_t low;
        uint64_t high;
    } luid;
    uint32_t gpu_index;
    std::unordered_map<uint64_t, GpuEngine> engines;
    VRAMInfo memory;
};

struct ProcessInfo {
    uint32_t pid;
    std::string process_name;
    std::unordered_map<uint32_t, GpuInfo> gpus;
    size_t memory_usage;
    size_t memory_available; // system ram
    struct {
        double user_cpu_usage;
        double kernel_cpu_usage;
        double total_cpu_usage;
    } cpu;
};

enum GpuMetric_Type : uint8_t {
    GpuMetric_Unknown = 0,
    GpuMetric_Dedicated_Vram = 1,
    GpuMetric_Shared_Vram = 2,
    GpuMetric_Engine_Utilization = 3,
};

enum CpuMetric_Type : uint8_t {
    CpuMetric_Unknown = 0,
    CpuMetric_User_Time = 1,
    CpuMetric_Priviledged_Time = 2,
    CpuMetric_Total_Time = 3,
};

inline auto getCurrentlyUsedGpu = [](const ProcessInfo& info) -> GpuInfo 
{
    auto gpuIt = std::ranges::find_if(info.gpus, [](auto& gpuEntry) {
        auto& [gpuId, gpu] = gpuEntry;
        return std::ranges::find_if(gpu.engines, [](auto& engEntry) {
            const auto& [idx, eng] = engEntry;
            return eng.engine_type == "3D" && eng.utilization_percentage > 0.0f;
        }) != gpu.engines.end();
    });

    return gpuIt != info.gpus.end()
        ? 
        gpuIt->second
        : GpuInfo{};
};

inline auto gpuPercentage = [](const GpuInfo& gpu) -> float
{
    if (auto it = std::ranges::find_if(gpu.engines,
        [](const auto& pair) {
            const auto& [key, eng] = pair;
            return eng.engine_type == "3D" &&
                eng.utilization_percentage > 0.0f;
        });
        it != gpu.engines.end())
    {
        return it->second.utilization_percentage;
    }
    return 0.0f;
};

inline auto gpuVideoPercentage = [](const GpuInfo& gpu) -> float
{
    if (auto it = std::ranges::find_if(gpu.engines,
        [](const auto& pair) {
            const auto& [key, eng] = pair;
            return eng.engine_type.find("Encode") != std::string::npos || eng.engine_type.find("Codec") != std::string::npos && eng.utilization_percentage > 0.0f;
        });
        it != gpu.engines.end())
    {
        return it->second.utilization_percentage;
    }
    return 0.0f;
};

class TaskMonitor {
public:
    explicit TaskMonitor();

    [[nodiscard]] auto Processes() const -> std::unordered_map<uint32_t, ProcessInfo> { return process_list_; }

    auto Initialize() -> void;
    auto Destroy() -> void;
    auto Update() -> void;
	auto GetProcessInfoByPid(uint32_t pid) -> ProcessInfo;
private:
    auto mapProcessesToPid(PDH_HCOUNTER counter) -> void;
    auto calculateGpuMetricFromCounter(PDH_HCOUNTER counter, GpuMetric_Type type) -> void;
    auto calculateCpuMetricFromCounter(PDH_HCOUNTER counter, CpuMetric_Type type) -> void;
    auto calculateMemoryMetricFromCounter(PDH_HCOUNTER counter) -> void;

    std::unordered_map<uint32_t, ProcessInfo> process_list_;
    std::unordered_map<std::string, uint32_t> process_map_;
    PDH_HQUERY pdh_query_;
    PDH_HCOUNTER pdh_processes_id_counter_;
	PDH_HCOUNTER pdh_dedicated_vram_counter_;
    PDH_HCOUNTER pdh_shared_vram_counter_;
	PDH_HCOUNTER pdh_gpu_utilization_counter_;
	PDH_HCOUNTER pdh_user_process_time_;
    PDH_HCOUNTER pdh_kernel_process_time_;
    PDH_HCOUNTER pdh_total_process_time_;
    PDH_HCOUNTER pdh_process_memory_;
    SYSTEM_INFO system_info_;
    MEMORYSTATUSEX system_memory_;
    IDXGIFactory6* dxgi_factory_;
    bool pdh_available_;
};