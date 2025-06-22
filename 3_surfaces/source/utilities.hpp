#ifndef SOURCE_UTILITIES_HPP
#define SOURCE_UTILITIES_HPP
#include <vulkan/vulkan_core.h>
#include <cstdint>
#include <vector>

const std::vector<const char *> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct QueueFamilyIndices
{

    int32_t graphics_family = -1;
    int32_t presentation_family = -1;

};

inline bool 
queue_family_indices_valid(QueueFamilyIndices *queue_family_indices)
{

    return  queue_family_indices->graphics_family >= 0 &&
            queue_family_indices->presentation_family >= 0;

}

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surface_capabilities;      // Surface properties, eg image size, etc.
    std::vector<VkSurfaceFormatKHR> formats;            // Surface image formats. RGBA sizeof each...
    std::vector<VkPresentModeKHR> presentation_modes;   // How they're presented... mailbox.
};

struct SwapChainImage
{

    VkImage image;
    VkImageView view;

};

#endif
