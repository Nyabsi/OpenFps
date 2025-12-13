#pragma once

#include <memory>
#include <atomic>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include <openvr.h>

#include "Overlay.hpp"

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

struct Vulkan_Surface
{
    static constexpr uint32_t ImageCount = 3;

    uint32_t width;
    uint32_t height;
    VkSurfaceFormatKHR texture_format;

    VkQueue queue;
    uint32_t frame_index;


    VkCommandPool command_pools[ImageCount];
    VkCommandBuffer command_buffers[ImageCount];
    VkImage textures[ImageCount];
    VkImageView texture_views[ImageCount];
    VkDeviceMemory texture_memories[ImageCount];
    VkFence fences[ImageCount];
    bool first_use[ImageCount];

    Vulkan_Surface()
    {
        memset(this, 0, sizeof(*this));
        frame_index = 0;
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

    auto SetupSurface(Overlay* overlay, uint32_t width, uint32_t height, VkSurfaceFormatKHR format) -> void;
    auto RenderSurface(ImDrawData* draw_data, Overlay* overlay) -> void;
    auto DestroySurface(Vulkan_Surface* surface) const -> void;

    auto Destroy() const -> void;
private:

    VkInstance vulkan_instance_;
    VkPhysicalDevice vulkan_physical_device_;
    std::atomic<uint32_t> vulkan_queue_family_;
    VkAllocationCallbacks* vulkan_allocator_;
    VkDevice vulkan_device_;
    VkQueue vulkan_queue_;
    VkDescriptorPool vulkan_descriptor_pool_;
    VkPipelineCache vulkan_pipeline_cache_;
    std::vector<std::string> vulkan_instance_extensions_;
    std::vector<std::string> vulkan_device_extensions_;
    VkDebugReportCallbackEXT debug_report_;
    std::vector<VkPhysicalDevice> device_list_;

    // Vulkan function wrappers
    PFN_vkCmdBeginRenderingKHR f_vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR f_vkCmdEndRenderingKHR;
};

extern VulkanRenderer* g_vulkanRenderer;