#pragma once

#include <memory>
#include <atomic>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include <openvr.h>

#include "VrOverlay.h"

struct Vulkan_Frame;
struct Vulkan_FrameSemaphore;

struct Vulkan_Frame
{
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkImage backbuffer;
    VkImageView backbuffer_view;
    VkFramebuffer framebuffer;
};

struct Vulkan_FrameSemaphore
{
    VkSemaphore image_acquired_semaphore;
    VkSemaphore render_complete_semaphore;
};

struct Vulkan_Window
{
    uint32_t width;
    uint32_t height;
    VkSwapchainKHR swapchain;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkRenderPass render_pass;
    VkPipeline pipeline;
    bool clear_enable;
    VkClearValue clear_value;
    uint32_t frame_index;
    uint32_t image_count;
    uint32_t semaphore_count;
    uint32_t semaphore_index;
    std::vector<Vulkan_Frame> frames;
    std::vector<Vulkan_FrameSemaphore> semaphores;
    bool is_minimized;

    Vulkan_Window()
    {
        memset((void*)this, 0, sizeof(*this));
    }
};

struct Vulkan_Overlay 
{
    uint32_t width;
    uint32_t height;
    VkSurfaceFormatKHR texture_format;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkImage texture;
    VkImageView texture_view;
    VkDeviceMemory texture_memory;
    VkQueue queue;
    bool clear_enable;
    VkClearValue clear_value;

    Vulkan_Overlay()
    {
        memset((void*)this, 0, sizeof(*this));
    }
};

class VulkanRenderer {
public:
    explicit VulkanRenderer();
    auto Initialize() -> void;

    [[nodiscard]] auto Instance() const -> VkInstance { return vulkan_instance_; }
    [[nodiscard]] auto PhysicalDevice() const -> VkPhysicalDevice { return vulkan_physical_device_; }
    [[nodiscard]] auto QueueFamily() const -> uint32_t { return vulkan_queue_family_; }
    [[nodiscard]] auto Allocator() const -> VkAllocationCallbacks* { return vulkan_allocator_; }
    [[nodiscard]] auto Device() const -> VkDevice { return vulkan_device_; }
    [[nodiscard]] auto Queue() const -> VkQueue { return vulkan_queue_; }
    [[nodiscard]] auto DescriptorPool() const -> VkDescriptorPool { return vulkan_descriptor_pool_; }
    [[nodiscard]] auto PipelineCache() const -> VkPipelineCache { return vulkan_pipeline_cache_; }
    [[nodiscard]] auto MinimumConcurrentImageCount() const -> uint32_t { return minimum_concurrent_image_count_; }
    [[nodiscard]] auto ShouldRebuildSwapchain() const -> bool { return should_rebuild_swapchain_; }

    auto SetupWindow(Vulkan_Window* window, VkSurfaceKHR surface, uint32_t width, uint32_t height) -> void;
    auto SetupOverlay(uint32_t width, uint32_t height, VkSurfaceFormatKHR format) -> void;
    auto SetupSwapchain(Vulkan_Window* window, uint32_t width, uint32_t height) -> void;
    // ImGui renderer helpers
    auto RenderWindow(ImDrawData* draw_data, Vulkan_Window* window) -> void;
    auto RenderOverlay(ImDrawData* draw_data, VrOverlay*& overlay) -> void;

    auto Present(Vulkan_Window* window) -> void;

    auto DestroyWindow(Vulkan_Window* window) const -> void;
    auto DestroyOverlay(Vulkan_Overlay* vulkan_overlay) const -> void;
    auto Destroy() -> void;
private:
    
    auto DestroyFrames(Vulkan_Window* window) const -> void;

    VkInstance vulkan_instance_;
    VkPhysicalDevice vulkan_physical_device_;
    std::atomic<uint32_t> vulkan_queue_family_;
    VkAllocationCallbacks* vulkan_allocator_;
    VkDevice vulkan_device_;
    VkQueue vulkan_queue_;
    VkDescriptorPool vulkan_descriptor_pool_;
    VkPipelineCache vulkan_pipeline_cache_;
    std::atomic<uint32_t> minimum_concurrent_image_count_;
    std::atomic<bool> should_rebuild_swapchain_;
    std::vector<std::string> vulkan_instance_extensions_;
    std::vector<std::string> vulkan_device_extensions_;
    VkDebugReportCallbackEXT debug_report_;
    std::vector<VkPhysicalDevice> device_list_;
    std::atomic<bool> should_enable_dynamic_rendering_;
    std::unique_ptr<Vulkan_Overlay> vulkan_overlay_;

    // Vulkan function wrappers
    PFN_vkCmdBeginRenderingKHR f_vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR f_vkCmdEndRenderingKHR;
};