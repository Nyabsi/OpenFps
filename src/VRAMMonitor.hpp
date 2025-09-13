#pragma once

#include <Windows.h>
#include <pdh.h>
#include <string>
#include <unordered_map>
#include <stdint.h>

struct ProcessVRAMInfo {
    uint32_t pid;
    std::string process_name;
    struct {
        uint32_t low;
        uint64_t high;
    } luid;
    uint32_t gpu_index;
    size_t dedicated_usage;
    size_t shared_usage;
};

struct VRAMInfo {
    size_t dedicated_usage;
    size_t shared_usage;
    size_t dedicated_available;
    size_t shared_available;
};

class VRAMMonitor {
public:
    explicit VRAMMonitor();
    auto Initialize() -> void;
    auto Update() -> void;
	auto GetByPid(uint64_t pid) -> ProcessVRAMInfo;
	auto GetUsageByGpuIndex(uint32_t index) -> VRAMInfo;
private:
    std::unordered_map<uint64_t, ProcessVRAMInfo> process_list_;
    PDH_HQUERY pdh_query_;
	PDH_HCOUNTER pdh_dedicated_vram_counter_;
    PDH_HCOUNTER pdh_shared_vram_counter_;
};