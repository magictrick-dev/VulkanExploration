#include "utilities.hpp"
#include <utility>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <VkRenderer.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <set>

static VKAPI_ATTR VkBool32 VKAPI_CALL 
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
        VkDebugUtilsMessageTypeFlagsEXT messageType, 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
        void* pUserData) 
{

    std::cerr 
        << "[" << pCallbackData->pMessageIdName << "] "
        << pCallbackData->pMessage << std::endl;
    return VK_TRUE;

}

VulkanRenderer::
VulkanRenderer()
{

}

VulkanRenderer::
~VulkanRenderer()
{

}

int32_t VulkanRenderer::
initialize(GLFWwindow *window)
{


    this->window = window;

    try
    {

        this->create_instance();
        this->create_surface();
        this->get_physical_device();
        this->create_logical_device();
        this->create_swapchain();

    }
    catch (const std::runtime_error &e)
    {

        printf("VkError: %s\n", e.what());
        return VK_RENDERER_INIT_CREATE_FAIL;

    }

    return VK_RENDERER_INIT_SUCCESS;

}

void VulkanRenderer::
deinitialize()
{

    this->destroy_swapchain();
    this->destroy_surface();
    this->destroy_logical_device();
    this->destroy_instance();

}

void VulkanRenderer::
create_instance()
{

    // Enable validation layers.
    this->validation_layers.push_back("VK_LAYER_KHRONOS_validation");

    if (!this->check_validation_support()) 
        throw std::runtime_error("Validation layers requested aren't available.");
    
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity =
        //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        ;
    debugCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
        ;
    debugCreateInfo.pfnUserCallback = debugCallback;

    // Application information for a Vulkan instance.
    // NOTE(Chris): Mostly for developer convenience.
    VkApplicationInfo application_info = {};
    application_info.sType                  = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName       = "Vulkan Probe";
    application_info.applicationVersion     = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName            = "No Engine";
    application_info.engineVersion          = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion             = VK_API_VERSION_1_3;

    // Creation information for a Vulkan instance.
    VkInstanceCreateInfo create_info = {};
    create_info.sType                       = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo            = &application_info;
    create_info.enabledLayerCount           = static_cast<uint32_t>(this->validation_layers.size());
    create_info.ppEnabledLayerNames         = this->validation_layers.data();
    create_info.pNext                       = &debugCreateInfo;


    auto createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (createFunc) {
        if (createFunc(instance, &debugCreateInfo, nullptr, &this->debug_messenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger");
        }
    }

    // Generate list to hold instance extensions.
    std::vector<const char*> instance_extensions;

    uint32_t glfw_extension_count = 0; // Basically what extensions GLFW requires.
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    for (uint64_t i = 0; i < glfw_extension_count; ++i)
    {

        instance_extensions.push_back(glfw_extensions[i]);

    }

    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Validate if the extensions are properly supported.
    if (!this->check_instance_extension_support(instance_extensions))
        throw std::runtime_error("VkInstance does not support GLFW required instance extension.");

    create_info.enabledExtensionCount       = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames     = instance_extensions.data();

    // TODO(Chris): Set up validation layers that we will use.
    create_info.enabledLayerCount           = 0;
    create_info.ppEnabledLayerNames         = NULL;

    VkResult result = vkCreateInstance(&create_info, nullptr, &this->instance);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create VkInstance.");

}

void VulkanRenderer::
destroy_instance()
{

    vkDestroyInstance(this->instance, nullptr);

}

bool VulkanRenderer::
check_instance_extension_support(std::vector<const char*>& check_extensions)
{

    // Query for the number of extensions.
    uint32_t extensions_supported = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensions_supported, nullptr);

    // Fetch the extensions.
    std::vector<VkExtensionProperties> extensions(extensions_supported);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensions_supported, extensions.data());

    for (auto check : check_extensions)
    {

        bool has_extension = false;
        for (auto ext : extensions)
        {

            if (strcmp(ext.extensionName, check) == 0)
            {
                has_extension = true;
                break;
            }

        }
    
        if (has_extension == false) return false;

    }

    return true;

}

bool VulkanRenderer::           
check_device_extension_support(VkPhysicalDevice &device)
{

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    if (extension_count == 0) return false;

    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());

    for (const auto device_extension : device_extensions)
    {

        bool has_extension = false;
        for (const auto extension : extensions)
        {

            if (strcmp(device_extension, extension.extensionName) == 0)
            {
                has_extension = true;
                break;
            }

        }

        if (!has_extension) return false;

    }

    return true;

}


void VulkanRenderer::
get_physical_device()
{

    // Query number of devices.
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(this->instance, &device_count, nullptr);

    if (device_count == 0) throw std::runtime_error("Unable to find any physical devices which support Vk.");

    // Get the physical devices.
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(this->instance, &device_count, devices.data());

    for (auto device : devices)
    {

        if (this->check_device_suitability(device))
        {
            this->main_device.physical_device = device;
            break;
        }

    }


}

bool VulkanRenderer::
check_device_suitability(VkPhysicalDevice &device)
{

    /*
    // NOTE(Chris): Check properties and features of the device to see if the device is what we want.
    VkPhysicalDeviceProperties device_propertes = {};
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features = {};
    vkGetPhysicalDeviceFeatures(device, &device_features);
    */

    QueueFamilyIndices indices = this->get_queue_families(device);

    bool extensions_supported = this->check_device_extension_support(device);
    bool swap_chain_valid = false;
    if (extensions_supported)
    {

        SwapChainDetails details = this->get_swapchain_details(device);
        swap_chain_valid = !details.formats.empty() && !details.presentation_modes.empty();

    }

    return queue_family_indices_valid(&indices) && extensions_supported && swap_chain_valid;

}

QueueFamilyIndices VulkanRenderer::
get_queue_families(VkPhysicalDevice &device) const
{

    QueueFamilyIndices indices = {};
    
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_list(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_family_list.data());

    // Go through each family and check if has at least one required type of queue.
    int32_t i = 0;
    for (const auto &queue_family : queue_family_list)
    {

        if (queue_family.queueCount > 0 && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {

            indices.graphics_family = i;

        }

        VkBool32 presentation_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, this->surface, &presentation_support);
        if (queue_family.queueCount > 0 && presentation_support)
        {

            indices.presentation_family = i;

        }

        if (queue_family_indices_valid(&indices))
        {
            break;
        }

        i++;

    }

    return std::move(indices);


}

void VulkanRenderer::           
create_logical_device()
{

    QueueFamilyIndices indices = this->get_queue_families(this->main_device.physical_device);

    // Queue creation information and set for family indices.
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<int> queue_family_indices = { indices.graphics_family, indices.presentation_family };

    for (auto queue_family_index : queue_family_indices)
    {

        // Queues the logical device needs to create.
        float priority = 1.0f; // Priority range is (0.0f to 1.0f).
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType                 = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex      = queue_family_index;
        queue_create_info.queueCount            = 1;
        queue_create_info.pQueuePriorities      = &priority; // Assign queue priority.

        queue_create_infos.push_back(queue_create_info);

    }

    // Physical devices features the logical device uses.
    VkPhysicalDeviceFeatures device_features = {};

    // Create the logical device.
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount     = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos        = queue_create_infos.data();
    device_create_info.enabledExtensionCount    = static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames  = device_extensions.data();
    device_create_info.enabledLayerCount        = static_cast<uint32_t>(this->validation_layers.size());
    device_create_info.ppEnabledLayerNames      = this->validation_layers.data();
    device_create_info.pEnabledFeatures         = &device_features;

    VkResult result = vkCreateDevice(this->main_device.physical_device, &device_create_info, 
            NULL, &this->main_device.logical_device);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create logical device.");

    // Get logical device queue family at index 0.
    vkGetDeviceQueue(this->main_device.logical_device, indices.graphics_family, 0, &this->graphics_queue);
    vkGetDeviceQueue(this->main_device.logical_device, indices.presentation_family, 0, &this->presentation_queue);

}

void VulkanRenderer::          
destroy_logical_device()
{

    vkDestroyDevice(this->main_device.logical_device, nullptr);

}

bool VulkanRenderer::           
check_validation_support()
{

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (auto layer : this->validation_layers)
    {

        bool layer_found = false;
        for (const auto &layer_properties : available_layers)
        {

            if (strcmp(layer, layer_properties.layerName) == 0)
            {
                layer_found = true;
                break;
            }

        }

        if (!layer_found) return false;

    }

    return true;

}


void VulkanRenderer::           
create_surface()
{

    // Creates surface via GLFW for us.
    VkResult result = glfwCreateWindowSurface(this->instance, this->window, nullptr, &this->surface);
    if (result != VK_SUCCESS) throw std::runtime_error("Unable to create GLFW VkSurface.");

}

void VulkanRenderer::
destroy_surface()
{

    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);

}

SwapChainDetails VulkanRenderer::       
get_swapchain_details(VkPhysicalDevice &device) const
{

    SwapChainDetails details = {};

    // Get capabilities.
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, this->surface, &details.surface_capabilities);

    // Get formats.
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->surface, &format_count, nullptr);
    if (format_count != 0)
    {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->surface, &format_count, details.formats.data());
    }

    // Get presentation modes.
    uint32_t presentation_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->surface, &presentation_count, nullptr);
    if (presentation_count != 0)
    {

        details.presentation_modes.resize(presentation_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, this->surface,
                &presentation_count, 
                details.presentation_modes.data());

    }

    return std::move(details);

}

void VulkanRenderer::
create_swapchain()
{

    // Get swap chain details.
    SwapChainDetails details = this->get_swapchain_details(this->main_device.physical_device);

    // 1. Choose best surface format.
    VkSurfaceFormatKHR surface_format = this->choose_optimal_surface_format(details.formats);

    // 2. Choose best presentation mode.
    VkPresentModeKHR present_mode = this->choose_optimal_presentation_mode(details.presentation_modes);

    // 3. Choose swap chain image resolution.
    VkExtent2D extents = this->choose_swap_extent(details.surface_capabilities);

    // 4. How many images in swap chain. Add one more than min for triple buffering.
    uint32_t image_count = details.surface_capabilities.minImageCount + 1;

    if (details.surface_capabilities.maxImageCount < image_count 
            && details.surface_capabilities.maxImageCount > 0)
    {
        image_count = details.surface_capabilities.maxImageCount;
    }

    // Create the swap chain.
    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType                = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface              = surface;
    swap_chain_create_info.imageFormat          = surface_format.format;
    swap_chain_create_info.imageColorSpace      = surface_format.colorSpace;
    swap_chain_create_info.presentMode          = present_mode;
    swap_chain_create_info.imageExtent          = extents;
    swap_chain_create_info.minImageCount        = image_count;
    swap_chain_create_info.imageArrayLayers     = 1;
    swap_chain_create_info.imageUsage           = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Potentially depth?
    swap_chain_create_info.preTransform         = details.surface_capabilities.currentTransform;
    swap_chain_create_info.compositeAlpha       = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Blending.
    swap_chain_create_info.clipped              = VK_TRUE;

    
    // Get queue family indices.
    QueueFamilyIndices indices = this->get_queue_families(this->main_device.physical_device);

    // If graphics & presentation families are different, then swapchain must let images be shared.
    if (indices.graphics_family != indices.presentation_family)
    {

        uint32_t queue_family_indices[2] { 
            static_cast<uint32_t>(indices.graphics_family),
            static_cast<uint32_t>(indices.presentation_family)
        };

        swap_chain_create_info.imageSharingMode         = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount    = 2;
        swap_chain_create_info.pQueueFamilyIndices      = queue_family_indices; // Array of queues to share.

    }

    // Shared mode.
    else
    {

        swap_chain_create_info.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
        swap_chain_create_info.queueFamilyIndexCount    = 0;
        swap_chain_create_info.pQueueFamilyIndices      = nullptr;

    }

    swap_chain_create_info.oldSwapchain         = nullptr; // If old swapchain has been destroyed, this replaces it.
                                                           
    VkResult result = vkCreateSwapchainKHR(this->main_device.logical_device, &swap_chain_create_info, 
            nullptr, &this->swapchain);

    vkDestroyDevice(VK_NULL_HANDLE, nullptr);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create a swapchain.");

    this->swapchain_format = surface_format.format;
    this->swapchain_extent = extents;

    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(this->main_device.logical_device, this->swapchain, &swapchain_image_count, nullptr);
    
    std::vector<VkImage> images(swapchain_image_count);
    vkGetSwapchainImagesKHR(this->main_device.logical_device, this->swapchain, &swapchain_image_count, images.data());

    for (VkImage image : images)
    {

        SwapChainImage swapchain_image = {};
        swapchain_image.image = image;
        swapchain_image.view = this->create_image_view(image, this->swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT);       

        this->swapchain_images.push_back(swapchain_image);

    }

}

void VulkanRenderer::
destroy_swapchain()
{

    for (auto view : this->swapchain_images)
    {

        vkDestroyImageView(this->main_device.logical_device, view.view, nullptr);

    }

    vkDestroySwapchainKHR(this->main_device.logical_device, this->swapchain, nullptr);

}

VkSurfaceFormatKHR VulkanRenderer::     
choose_optimal_surface_format(std::vector<VkSurfaceFormatKHR> &formats)
{

    // NOTE(Chris): We want:
    //              VK_FORMAT_R8G8B8A8_UNORM (backup BGR)
    //              VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {

        return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    }

    for (const auto &format : formats)
    {

        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {

            return format;

        }

    }

    // NOTE(Chris): Fallback, this could just blow up lol.
    std::cerr << "Bad format, or unfounded." << std::endl;
    return formats[0];

}

VkPresentModeKHR VulkanRenderer::       
choose_optimal_presentation_mode(std::vector<VkPresentModeKHR> &modes)
{

    for (const auto &mode : modes)
    {

        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;

    }

    // NOTE(Chris): This is a safe backup, Vulkan specifies this must exist.
    return VK_PRESENT_MODE_FIFO_KHR;

}

VkExtent2D VulkanRenderer::             
choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities)
{

    // Not varying.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {

        return capabilities.currentExtent;

    }

    // Varying.
    else
    {

        // Get window size.
        int32_t width, height;
        glfwGetFramebufferSize(this->window, &width, &height);

        // Create new extent using window size.
        VkExtent2D extents  = {};
        extents.width       = static_cast<uint32_t>(width);
        extents.height      = static_cast<uint32_t>(height);
        extents.width       = std::max(capabilities.minImageExtent.width, 
                                    std::min(capabilities.maxImageExtent.width, extents.width));
        extents.height      = std::max(capabilities.minImageExtent.height, 
                                    std::min(capabilities.maxImageExtent.height, extents.height));

        return extents;

    }

}

VkImageView VulkanRenderer::
create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{

    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image              = image;
    view_create_info.viewType           = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format             = format;
    view_create_info.components.r       = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g       = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b       = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a       = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresources allow the view to view only a part of an image.
    view_create_info.subresourceRange.aspectMask        = aspect_flags; // Which aspect to view? (COLOR_BIT)
    view_create_info.subresourceRange.baseMipLevel      = 0;
    view_create_info.subresourceRange.levelCount        = 1;
    view_create_info.subresourceRange.baseArrayLayer    = 0;
    view_create_info.subresourceRange.layerCount        = 1;

    // Create the image view and return it.
    VkImageView view;
    VkResult result = vkCreateImageView(this->main_device.logical_device, &view_create_info, nullptr, &view);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create an image view.");

    return std::move(view);

}
