#pragma once

#include <vector>

#include <imgui.h>

#include <core/TaskMonitor.hpp>
#include <core/Settings.hpp>

#include <overlay/Overlay.hpp>

class DashboardOverlay : public Overlay
{
public:
    explicit DashboardOverlay();

    auto Render() -> bool override;
    auto Update() -> void override;
    auto Destroy() -> void;
private:
    TaskMonitor task_monitor_;
    Settings settings_;
};