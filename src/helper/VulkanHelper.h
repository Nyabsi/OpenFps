#pragma once

#include <vector>
#include <sstream>

#include <vulkan/vulkan.h>
#include <openvr.h>

#ifdef _WIN32
#include <Windows.h> // MessageBoxA
#endif

#include <config.hpp>

#define VK_VALIDATE_RESULT(e)                                  \
    if (e != VK_SUCCESS)                                       \
        fprintf(stderr, "[Vulkan] Error: VkResult = %d\n", e); \
    if (e > 0)                                                 \
        assert(e);                                             \

static auto IsVulkanInstanceExtensionAvailable(std::string extension) -> bool 
{
    auto IsExtensionAvailable = [](const std::vector<VkExtensionProperties>& properties, std::string extension) {
        for (const VkExtensionProperties& p : properties)
            if (strcmp(p.extensionName, extension.c_str()) == 0)
                return true;
        return false;
    };

    uint32_t extension_properties_count = {};
    std::vector<VkExtensionProperties> extension_properties = {};

    vkEnumerateInstanceExtensionProperties(nullptr, &extension_properties_count, nullptr);

    if (extension_properties_count > 0) {
        extension_properties.resize(extension_properties_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_properties_count, extension_properties.data());
    }
    else {
        std::exit(EXIT_FAILURE);
    }

    return IsExtensionAvailable(extension_properties, extension);
}

static auto IsVulkanDeviceExtensionAvailable(const VkPhysicalDevice& physical_device, std::string extension) -> bool 
{
    auto IsExtensionAvailable = [](const std::vector<VkExtensionProperties>& properties, std::string extension) {
        for (const VkExtensionProperties& p : properties)
            if (strcmp(p.extensionName, extension.c_str()) == 0)
                return true;
        return false;
    };

    uint32_t extension_properties_count = {};
    std::vector<VkExtensionProperties> extension_properties = {};

    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_properties_count, nullptr);

    if (extension_properties_count > 0) {
        extension_properties.resize(extension_properties_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_properties_count, extension_properties.data());
    }
    else {
        std::exit(EXIT_FAILURE);
    }

    return IsExtensionAvailable(extension_properties, extension);
}

static auto GetVulkanInstanceExtensionsRequiredByOpenVR() -> std::vector<std::string>
{
    std::vector<std::string> result{};

    if (!vr::VRCompositor())
    {
        std::exit(EXIT_FAILURE);
    }

    uint32_t buffer_len = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);
    if (buffer_len > 0) 
    {
        std::vector<char> buffer(buffer_len + 1);
        vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(buffer.data(), buffer_len);
        buffer[buffer_len] = '\0';

        std::string token{};
        std::istringstream token_stream(buffer.data());
        while (std::getline(token_stream, token, ' ')) {
            if (IsVulkanInstanceExtensionAvailable(token)) {
                result.push_back(token);
            } else {
#ifdef _WIN32
                char error_message[512] = {};
                snprintf(error_message, 512, "ERROR! %s instance extension asked by OpenVR was NOT available.\n", token.c_str());
                MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
                printf("ERROR! %s instance extension asked by OpenVR was NOT available.\n", token.c_str());
                std::exit(EXIT_FAILURE);
            }
        }
    } else {
        std::exit(EXIT_FAILURE);
    }

    return result;
}

static auto GetVulkanDeviceExtensionsRequiredByOpenVR(const VkPhysicalDevice& device) -> std::vector<std::string> 
{
    std::vector<std::string> result{};

    if (!vr::VRCompositor()) {
        std::exit(EXIT_FAILURE);
    }

    uint32_t buffer_len = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(device, nullptr, 0);
    if (buffer_len > 0) {
        std::vector<char> buffer(buffer_len + 1);
        vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(device, buffer.data(), buffer_len);
        buffer[buffer_len] = '\0';

        std::string token{};
        std::istringstream token_stream(buffer.data());
        while (std::getline(token_stream, token, ' ')) {
            if (IsVulkanDeviceExtensionAvailable(device, token.data())) {
                result.push_back(token);
            } else {
#ifdef _WIN32
                char error_message[512] = {};
                snprintf(error_message, 512, "ERROR! %s device extension asked by OpenVR was NOT available\n", token.c_str());
                MessageBoxA(NULL, error_message, APP_NAME, MB_OK);
#endif
                printf("ERROR! %s device extension asked by OpenVR was NOT available\n", token.c_str());
                std::exit(EXIT_FAILURE);
            }
        }
    } else {
        std::exit(EXIT_FAILURE);
    }

    return result;
}