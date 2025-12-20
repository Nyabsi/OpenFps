#include "TaskMonitor.hpp"

#include <Windows.h>
#include <pdh.h>
#include <stdexcept>
#include <PdhMsg.h>
#include <sstream>
#include <vector>
#include <dxgi1_6.h>
#include <thread>

#include <config.hpp>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

TaskMonitor::TaskMonitor()
{
    process_list_.clear();
    process_map_.clear();

    pdh_query_ = { };

    pdh_processes_id_counter_ = { };
    pdh_dedicated_vram_counter_ = { };
    pdh_shared_vram_counter_ = { };
    pdh_gpu_utilization_counter_ = { };
    pdh_user_process_time_ = { };
    pdh_kernel_process_time_ = { };
    pdh_total_process_time_ = { };
    pdh_process_memory_ = { };
    system_info_ = { };
    system_memory_ = { };
    dxgi_factory_ = nullptr;
}

auto TaskMonitor::Initialize() -> void
{
    PDH_STATUS result = {};

    try {
        result = PdhOpenQueryA(NULL, 0, &pdh_query_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to open query through PdhOpenQueryA");

        result = PdhAddEnglishCounterA(pdh_query_, "\\Process(*)\\Id Process", 0, &pdh_processes_id_counter_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Id Process) through PdhAddCounterA");

        result = PdhAddEnglishCounterA(pdh_query_, "\\GPU Process Memory(*)\\Dedicated Usage", 0, &pdh_dedicated_vram_counter_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Dedicated Usage) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\GPU Process Memory(*)\\Shared Usage", 0, &pdh_shared_vram_counter_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Shared Usage) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\GPU Engine(*)\\Utilization Percentage", 0, &pdh_gpu_utilization_counter_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Utilization Percentage) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\Process(*)\\% User Time", 0, &pdh_user_process_time_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (User Time) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\Process(*)\\% Privileged Time", 0, &pdh_kernel_process_time_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Privileged Time) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\Process(*)\\% Processor Time", 0, &pdh_total_process_time_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Processor Time) through PdhAddCounterA");

        PdhAddEnglishCounterA(pdh_query_, "\\Process(*)\\Working Set", 0, &pdh_process_memory_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to register counter (Processor Time) through PdhAddCounterA");
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to initialize PDH counters.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        std::exit(EXIT_FAILURE);
    }

    GetSystemInfo(&system_info_);
    system_memory_.dwLength = sizeof(system_memory_);
    GlobalMemoryStatusEx(&system_memory_);
    CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&dxgi_factory_);
}

auto TaskMonitor::Destroy() -> void
{
    PdhCloseQuery(pdh_query_);

    PdhRemoveCounter(pdh_processes_id_counter_);
    PdhRemoveCounter(pdh_dedicated_vram_counter_);
    PdhRemoveCounter(pdh_shared_vram_counter_);
    PdhRemoveCounter(pdh_gpu_utilization_counter_);
    PdhRemoveCounter(pdh_user_process_time_);
    PdhRemoveCounter(pdh_kernel_process_time_);
    PdhRemoveCounter(pdh_total_process_time_);
    PdhRemoveCounter(pdh_process_memory_);

    system_info_ = { };
    system_memory_ = { };
    dxgi_factory_->Release();
}

auto TaskMonitor::Update() -> void
{
    process_list_.clear();
    process_map_.clear();

    PDH_STATUS result = {};

    try {
        result = PdhCollectQueryData(pdh_query_);
        if (result != ERROR_SUCCESS)
            throw std::runtime_error("Failed to collect query data through PdhCollectQueryData");
    }
    catch (std::exception& ex) {
#ifdef _WIN32
        char error_message[512] = {};
        snprintf(error_message, 512, "Failed to collect PDH counters.\nReason: %s\r\n", ex.what());
        MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
        printf("%s\n\n", ex.what());
        return;
    }

    mapProcessesToPid(pdh_processes_id_counter_);

    calculateGpuMetricFromCounter(pdh_dedicated_vram_counter_, GpuMetric_Dedicated_Vram);
    calculateGpuMetricFromCounter(pdh_shared_vram_counter_, GpuMetric_Shared_Vram);
    calculateGpuMetricFromCounter(pdh_gpu_utilization_counter_, GpuMetric_Engine_Utilization);

    calculateCpuMetricFromCounter(pdh_user_process_time_, CpuMetric_User_Time);
    calculateCpuMetricFromCounter(pdh_kernel_process_time_, CpuMetric_Priviledged_Time);
    calculateCpuMetricFromCounter(pdh_total_process_time_, CpuMetric_Total_Time);

    calculateMemoryMetricFromCounter(pdh_process_memory_);

    for (auto it = process_list_.begin(); it != process_list_.end(); ) {
        // If the process name is empty but allocates VRAM it's an system process
        // it's removed because Task Manager removes these processes as well.
        if (it->second.process_name.empty() || it->second.process_name.find("Idle") != std::string::npos || it->second.process_name.find("_Total") != std::string::npos) {
            it = process_list_.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto& [pid, process] : process_list_) {
        for (auto& [index, info] : process.gpus) {
            IDXGIAdapter1* adapter = nullptr;
            if (SUCCEEDED(dxgi_factory_->EnumAdapters1(index, &adapter))) {
                DXGI_ADAPTER_DESC1 desc = {};
                if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                    info.memory.dedicated_available = desc.DedicatedVideoMemory;
                    info.memory.shared_available = desc.SharedSystemMemory;
                }
                adapter->Release();
            }
        }

        process.cpu.user_cpu_usage /= system_info_.dwNumberOfProcessors;
        process.cpu.kernel_cpu_usage /= system_info_.dwNumberOfProcessors;
        process.cpu.total_cpu_usage /= system_info_.dwNumberOfProcessors;
        process.memory_available = system_memory_.ullTotalPhys;
    }
}

auto TaskMonitor::GetProcessInfoByPid(uint64_t pid) -> ProcessInfo
{
    return process_list_[pid];
}

auto TaskMonitor::mapProcessesToPid(PDH_HCOUNTER counter) -> void
{
    PDH_STATUS result = {};

    DWORD bufferSize = 0;
    DWORD itemCount = 0;

    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    std::vector<std::byte> buffer(bufferSize);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buffer.data());
    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != nullptr && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            process_list_[items[i].FmtValue.largeValue].process_name = items[i].szName;
            process_map_[items[i].szName] = items[i].FmtValue.largeValue;
        }
    }
}

auto TaskMonitor::calculateGpuMetricFromCounter(PDH_HCOUNTER counter, GpuMetric_Type type) -> void
{
    PDH_STATUS result = {};

    auto parseCounterToStruct = [&](const std::string& name, LONGLONG& value) -> void {
        std::stringstream stream(name);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(stream, token, '_'))
            tokens.push_back(token);

        uint32_t pid = 0;
        uint32_t engine_index = 0;
        int gpu_index = 0;

        uint64_t luid_low = 0;
        uint64_t luid_high = 0;
        std::string engine_type;

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "pid") {
                pid = std::stoul(tokens[++i]);
            }
            else if (tokens[i] == "luid") {
                luid_low = std::stoul(tokens[++i], nullptr, 16);
                luid_high = std::stoull(tokens[++i], nullptr, 16);
            }
            else if (tokens[i] == "phys") {
                gpu_index = std::stoi(tokens[++i]);
            }
            else if (tokens[i] == "eng") {
                engine_index = std::stoi(tokens[++i]);
            }
            else if (tokens[i] == "engtype") {
                engine_type = tokens[++i];
            }
        }

        auto& gpu = process_list_[pid].gpus[gpu_index];

        gpu.gpu_index = gpu_index;

        gpu.luid.low = luid_low;
        gpu.luid.high = luid_high;

        auto& eng = gpu.engines[engine_index];
        eng.engine_index = engine_index;

        if (!engine_type.empty())
            eng.engine_type = engine_type;

        switch (type)
        {
        case GpuMetric_Dedicated_Vram:
            gpu.memory.dedicated_vram_usage = value;
            break;

        case GpuMetric_Shared_Vram:
            gpu.memory.shared_vram_usage = value;
            break;

        case GpuMetric_Engine_Utilization:
            eng.utilization_percentage = value;
            break;
        }
    };

    DWORD bufferSize = 0;
    DWORD itemCount = 0;

    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    std::vector<std::byte> buffer(bufferSize);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buffer.data());
    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            parseCounterToStruct(items[i].szName, items[i].FmtValue.largeValue);
        }
    }
}

auto TaskMonitor::calculateCpuMetricFromCounter(PDH_HCOUNTER counter, CpuMetric_Type type) -> void
{
    PDH_STATUS result = {};

    DWORD bufferSize = 0;
    DWORD itemCount = 0;

    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    std::vector<std::byte> buffer(bufferSize);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buffer.data());
    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != nullptr && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            uint64_t pid = process_map_[items[i].szName];
            if (strcmp(items[i].szName, "Idle") != 0 && strcmp(items[i].szName, "_Total") != 0) {
                switch (type)
                {
                case CpuMetric_User_Time:
                    process_list_[pid].cpu.user_cpu_usage = items[i].FmtValue.doubleValue;
                    break;
                case CpuMetric_Priviledged_Time:
                    process_list_[pid].cpu.kernel_cpu_usage = items[i].FmtValue.doubleValue;
                    break;
                case CpuMetric_Total_Time:
                    process_list_[pid].cpu.total_cpu_usage = items[i].FmtValue.doubleValue;
                    break;
                }
            }
        }
    }
}

auto TaskMonitor::calculateMemoryMetricFromCounter(PDH_HCOUNTER counter) -> void
{
    PDH_STATUS result = {};

    DWORD bufferSize = 0;
    DWORD itemCount = 0;

    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Working Set) through PdhGetFormattedCounterArrayA");

    std::vector<std::byte> buffer(bufferSize);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM*>(buffer.data());
    result = PdhGetFormattedCounterArrayA(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != nullptr && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            uint64_t pid = process_map_[items[i].szName];
            if (strcmp(items[i].szName, "Idle") != 0 && strcmp(items[i].szName, "_Total") != 0) {
                process_list_[pid].memory_usage = items[i].FmtValue.doubleValue;
            }
        }
    }
}
