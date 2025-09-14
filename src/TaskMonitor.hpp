#pragma once

#include <Windows.h>
#include <pdh.h>
#include <string>
#include <unordered_map>
#include <stdint.h>

struct GpuEngine {
    uint32_t engine_index;
	std::string engine_type;
    float utilization_percentage;
};

struct ProcessInfo {
    uint32_t pid;
    std::string process_name;
    struct {
        struct {
            uint32_t low;
            uint64_t high;
        } luid;
        uint32_t gpu_index;
        float dedicated_vram_usage;
        float shared_vram_usage;
        std::unordered_map<uint64_t, GpuEngine> engines;
    } gpu;
    struct {
        size_t total_processes;
        std::unordered_map<uint64_t, float> utilization_percentages;
        float utilization_percentage;
    } cpu;
    struct {
        size_t total_usage; /* combined usage, including ALL sub-processes. */
    } memory;
};

struct VRAMInfo {
    size_t dedicated_vram_usage;
    size_t shared_vram_usage;
    size_t dedicated_available;
    size_t shared_available;
};

class TaskMonitor {
public:
    explicit TaskMonitor();
    auto Initialize() -> void;
    auto Update() -> void;
	auto GetProcessInfoByPid(uint64_t pid) -> ProcessInfo;
	auto GetVramUsageByGpuIndex(uint32_t index) -> VRAMInfo;
private:
    std::unordered_map<uint64_t, ProcessInfo> process_list_;
    PDH_HQUERY pdh_query_;
	PDH_HCOUNTER pdh_dedicated_vram_counter_;
    PDH_HCOUNTER pdh_shared_vram_counter_;
	PDH_HCOUNTER pdh_gpu_utilization_counter_;
	PDH_HCOUNTER pdh_cpu_utilization_counter_;
	PDH_HCOUNTER pdh_memory_usage_counter_;
};