#include "VRAMMonitor.hpp"

#include <Windows.h>
#include <pdh.h>
#include <stdexcept>
#include <PdhMsg.h>
#include <sstream>
#include <vector>
#include <dxgi1_6.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

VRAMMonitor::VRAMMonitor()
{
    process_list_.clear();

    pdh_query_ = { };
    pdh_dedicated_vram_counter_ = { };
    pdh_shared_vram_counter_ = { };
}

auto VRAMMonitor::Initialize() -> void
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

    PdhCollectQueryData(pdh_query_);
}

auto VRAMMonitor::Update() -> void
{
    PDH_STATUS result = {};

    result = PdhCollectQueryData(pdh_query_);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to open query through PdhOpenQueryA");

    auto parseCounterToStruct = [](const std::string &name, ProcessVRAMInfo& info) {
        std::stringstream stream(name);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(stream, token, '_')) {
            tokens.push_back(token);
        }

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "pid" && i + 1 < tokens.size()) {
                info.pid = std::stoul(tokens[i + 1]);
                i++;
            }
            else if (tokens[i] == "luid" && i + 2 < tokens.size()) {
                info.luid.low = std::stoull(tokens[i + 1], nullptr, 16);
                info.luid.high = std::stoull(tokens[i + 2], nullptr, 16);
                i += 2;
            }
            else if (tokens[i] == "phys" && i + 1 < tokens.size()) {
                info.gpu_index = std::stoi(tokens[i + 1]);
                i++;
            }
        }

        return info.pid != 0;
    };

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PDH_FMT_COUNTERVALUE_ITEM* items = nullptr;

    result = PdhGetFormattedCounterArrayA(pdh_dedicated_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_dedicated_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to get formatted counter array (Dedicated Usage) through PdhGetFormattedCounterArrayA");

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessVRAMInfo info = {};
            if (parseCounterToStruct(items[i].szName, info)) {
                info.dedicated_usage = items[i].FmtValue.largeValue;
                process_list_[info.pid] = info;
            }
        }
    }

    free(items);
    bufferSize = 0;

    result = PdhGetFormattedCounterArrayA(pdh_shared_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
    if (result != PDH_MORE_DATA)
        throw std::runtime_error("Failed to get formatted counter array size (Shared Usage) through PdhGetFormattedCounterArrayA");

    items = (PDH_FMT_COUNTERVALUE_ITEM*)malloc(bufferSize);
    result = PdhGetFormattedCounterArrayA(pdh_shared_vram_counter_, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
    if (result != ERROR_SUCCESS)
        throw std::runtime_error("Failed to get formatted counter array (Shared Usage) through PdhGetFormattedCounterArrayA");

    for (DWORD i = 0; i < itemCount; ++i) {
        if (items != NULL && items[i].FmtValue.CStatus == ERROR_SUCCESS) {
            ProcessVRAMInfo info = {};
            if (parseCounterToStruct(items[i].szName, info)) {
                if (process_list_.find(info.pid) != process_list_.end()) {
                    process_list_[info.pid].shared_usage = items[i].FmtValue.largeValue;
                }
            }
        }
    }

    free(items);
    bufferSize = 0;

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
}

auto VRAMMonitor::GetByPid(uint64_t pid) -> ProcessVRAMInfo
{
    for (const auto& [process_pid, info] : process_list_) {
        if (process_pid == pid)
            return info;
    }
    return {};
}

auto VRAMMonitor::GetUsageByGpuIndex(uint32_t index) -> VRAMInfo
{
    HRESULT result = {};

    VRAMInfo info = {};
    for (const auto& p : process_list_) {
        info.dedicated_usage += p.second.dedicated_usage;
        info.shared_usage += p.second.shared_usage;
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
