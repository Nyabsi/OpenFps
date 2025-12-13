#pragma once

#include <vector>

#include <imgui.h>

#include "Overlay.hpp"
#include "TaskMonitor.hpp"
#include "Settings.hpp"

class DashboardOverlay : public Overlay
{
public:
    explicit DashboardOverlay();

    auto Render() -> void override;
    auto Update() -> void override;
    auto Destroy() -> void;
private:
    TaskMonitor task_monitor_;
    Settings settings_;
};