#include "TaskMonitor.hpp"

#include <Windows.h>
#include <pdh.h>
#include <stdexcept>
#include <PdhMsg.h>
#include <sstream>
#include <vector>
#include <dxgi1_6.h>
#include <thread>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

TaskMonitor::TaskMonitor()
{
    process_list_.clear();

    pdh_query_ = { };
    pdh_dedicated_vram_counter_ = { };
    pdh_shared_vram_counter_ = { };
    pdh_gpu_utilization_counter_ = { };
    pdh_cpu_utilization_counter_ = { };
    pdh_memory_usage_counter_ = { };
}

auto TaskMonitor::Initialize() -> void
{
    PDH_STATUS result = {};

    result = PdhOpenQueryA(NULL, 0, &pdh_query_);
	if (result != ERROR_SUCCESS) 
        throw std::runtime_error("Failed to open query through PdhOpenQueryA");

    result = PdhAddCounterA(pdh_query_, "\\GPU Process Memory(*)\\Dedicated Usage", 0, &pdh_dedicated_vram_counter_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to register counter (Dedicated Usage) through PdhAddCounterA");

    PdhAddCounterA(pdh_query_, "\\GPU Process Memory(*)\\Shared Usage", 0, &pdh_shared_vram_counter_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to register counter (Shared Usage) through PdhAddCounterA");

    PdhAddCounterA(pdh_query_, "\\GPU Engine(*)\\Utilization Percentage", 0, &pdh_gpu_utilization_counter_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to register counter (Utilization Percentage) through PdhAddCounterA");

    PdhAddCounterA(pdh_query_, "\\Process(*)\\% Processor Time", 0, &pdh_cpu_utilization_counter_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to register counter (Processor Time) through PdhAddCounterA");

    PdhAddCounterA(pdh_query_, "\\Process(*)\\Working Set", 0, &pdh_memory_usage_counter_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to register counter (Working Set) through PdhAddCounterA");
}

auto TaskMonitor::Update() -> void
{
    PDH_STATUS result = {};

    result = PdhCollectQueryData(pdh_query_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to collect query data through PdhCollectQueryData");

    auto parseCounterToStruct = [](const std::string& name, ProcessInfo& info) {
        std::stringstream stream(name);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(stream, token, '_')) {
            tokens.push_back(token);
        }

        uint32_t engine_index = 0;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "pid" && i + 1 < tokens.size()) {
                info.pid = std::stoul(tokens[i + 1]);
                i++;
            }
            else if (tokens[i] == "luid" && i + 2 < tokens.size()) {
                info.gpu.luid.low = std::stoul(tokens[i + 1], nullptr, 16);
                info.gpu.luid.high = std::stoull(tokens[i + 2], nullptr, 16);
                i += 2;
            }
            else if (tokens[i] == "phys" && i + 1 < tokens.size()) {
                info.gpu.gpu_index = std::stoi(tokens[i + 1]);
                i++;
            }
            else if (tokens[i] == "eng" && i + 1 < tokens.size()) {
                engine_index = std::stoi(tokens[i + 1]);
                info.gpu.engines[engine_index].engine_index = engine_index;
                i++;
            }

            else if (tokens[i] == "engtype" && i + 1 < tokens.size()) {
                std::string type = tokens[i + 1];
                info.gpu.engines[engine_index].engine_type = type;
                break;
            }
        }

        return engine_index;
    };

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_FMT_COUNTERVALUE_ITEM* items = nullptr;

    result = PdhGetFormattedCounterArrayA(pdh_dedicated_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_dedicated_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessInfo info = {};
            parseCounterToStruct(items[i].szName, info);
            info.gpu.dedicated_vram_usage = items[i].FmtValue.largeValue;
            process_list_[info.pid] = info;
        }
    }

    free(items);
    bufferSize = 0;

    result = PdhGetFormattedCounterArrayA(pdh_shared_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Shared Usage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_shared_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessInfo info = {};
            parseCounterToStruct(items[i].szName, info);
            if (process_list_.find(info.pid) != process_list_.end()) {
                process_list_[info.pid].gpu.shared_vram_usage = items[i].FmtValue.largeValue;
            }
        }
    }

    free(items);
    bufferSize = 0;

    result = PdhGetFormattedCounterArrayA(pdh_gpu_utilization_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Utilization Percentage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_gpu_utilization_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessInfo info = {};
            auto idx = parseCounterToStruct(items[i].szName, info);
            info.gpu.engines[idx].utilization_percentage = items[i].FmtValue.largeValue;
            process_list_[info.pid].gpu.engines[idx] = info.gpu.engines[idx];
        }
    }

    auto pidToName = [](uint64_t pid) -> std::string {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
            return "";

        char process_path[MAX_PATH]{};
        DWORD path_len = MAX_PATH;
        std::string name = "";
        if (QueryFullProcessImageNameA(process, 0, process_path, &path_len)) {
            std::string path(process_path);
            size_t pos = path.find_last_of("\\/");
            name = (pos == std::string::npos) ? path : path.substr(pos + 1);
        }
        CloseHandle(process);
        return name;
    };

    for (auto it = process_list_.begin(); it != process_list_.end(); ) {
        it->second.process_name = pidToName(it->first);
        // If the process name is empty but allocates VRAM it's an system process
        // it's removed because Task Manager removes these processes as well.
        if (it->second.process_name.empty()) {
            it = process_list_.erase(it);
        }
        else {
            ++it;
        }
    }

    free(items);
    bufferSize = 0;

    result = PdhGetFormattedCounterArrayA(pdh_cpu_utilization_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Utilization Percentage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_cpu_utilization_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessInfo info = {};
            // name -> pid
            for (const auto& process : process_list_) {
                if (strstr(process.second.process_name.c_str(), items[i].szName)) {
                    info.pid = process.second.pid;
                    break;
                }
            }

            info.cpu = process_list_[info.pid].cpu;
            info.cpu.utilization_percentages[info.cpu.total_processes] = items[i].FmtValue.largeValue;
            info.cpu.total_processes += 1;
            
            if (info.pid > 0)
                process_list_[info.pid].cpu = info.cpu;
        }
    }

    free(items);
    bufferSize = 0;

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    float utilization_percentage = 0.0f;
    for (auto it = process_list_.begin(); it != process_list_.end(); ) {
		for (auto& u : it->second.cpu.utilization_percentages)
            utilization_percentage += u.second;
		it->second.cpu.utilization_percentage = utilization_percentage / system_info.dwNumberOfProcessors;
        it++;
    }
}

auto TaskMonitor::GetProcessInfoByPid(uint64_t pid) -> ProcessInfo
{
    for (const auto& [process_pid, info] : process_list_) {
        if (process_pid == pid)
            return info;
    }
    return {};
}

auto TaskMonitor::GetVramUsageByGpuIndex(uint32_t index) -> VRAMInfo
{
    HRESULT result = {};

    VRAMInfo info = {};
    for (const auto& p : process_list_) {
        info.dedicated_vram_usage += p.second.gpu.dedicated_vram_usage;
        info.shared_vram_usage += p.second.gpu.shared_vram_usage;
    }

    IDXGIFactory6* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&factory))) {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(index, &adapter))) {
            DXGI_ADAPTER_DESC1 desc = {};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                info.dedicated_available = desc.DedicatedVideoMemory;
                info.shared_available = desc.SharedSystemMemory;
            }
            adapter->Release();
        }
        factory->Release();
    }

    return info;
}
