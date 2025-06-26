#ifndef SOURCE_UTILITIES_HPP
#define SOURCE_UTILITIES_HPP
#include <vulkan/vulkan_core.h>
#include <cstdint>
#include <vector>
#include <fstream>

const int32_t max_frame_draws = 3;

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

static std::vector<char> read_file(const std::string &file_name)
{

    // Open the file.
    std::ifstream file(file_name, std::ios::binary | std::ios::ate);
    if (!file.is_open()) throw std::runtime_error("Failed to open file.");

    // Retrieves the size.
    uint64_t file_size = static_cast<uint64_t>(file.tellg());
    
    // File contents.
    std::vector<char> file_buffer(file_size);

    // Reset and then fill vector.
    file.seekg(0);
    file.read(file_buffer.data(), file_size);

    // Close stream.
    file.close();

    return std::move(file_buffer);

}

#endif
