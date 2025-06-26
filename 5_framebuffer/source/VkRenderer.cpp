#include "utilities.hpp"
#include <utility>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <VkRenderer.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <set>
#include <array>

static VKAPI_ATTR VkBool32 VKAPI_CALL 
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
        VkDebugUtilsMessageTypeFlagsEXT messageType, 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
        void* pUserData) 
{

    std::cerr 
        << "[" << pCallbackData->pMessageIdName << "] "
        << pCallbackData->pMessage << std::endl;
    return VK_FALSE;

}

VulkanRenderer::
VulkanRenderer()
{

}

VulkanRenderer::
~VulkanRenderer()
{

}

void VulkanRenderer::
draw()
{

    // Wait for the queue to clear.
    vkWaitForFences(this->main_device.logical_device, 1, &this->draw_fences[current_frame], 
            VK_FALSE, std::numeric_limits<uint64_t>::max()); // Wait for it to open.
    vkResetFences(this->main_device.logical_device, 1, &this->draw_fences[current_frame]); // Close the fence.

    // 1.   Get the next available image to render to.
    //      We don't want time out, say basically wait as long as possible.
    uint32_t image_index;
    vkAcquireNextImageKHR(this->main_device.logical_device, this->swapchain, 
            std::numeric_limits<uint64_t>::max(), this->images_available[current_frame], 
            VK_NULL_HANDLE, &image_index);

    // 2.   Submit command buffer to queue for execution, making sure it waits for 
    //      the image to be signalled.
    VkSubmitInfo submit_info = {};
    submit_info.sType                       = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount          = 1;
    submit_info.pWaitSemaphores             = &this->images_available[current_frame]; // Wait until color attachment output bit.
    
    VkPipelineStageFlags wait_stages[] // Corresponds to semahore count.
    {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    submit_info.pWaitDstStageMask           = wait_stages;
    submit_info.commandBufferCount          = 1;
    submit_info.pCommandBuffers             = &this->command_buffers[image_index];
    submit_info.signalSemaphoreCount        = 1;
    submit_info.pSignalSemaphores           = &this->renders_finished[current_frame];

    // Open the fence once the queue is completed.
    VkResult result = vkQueueSubmit(this->graphics_queue, 1, &submit_info, this->draw_fences[current_frame]); // Things happen now!
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to submit command queue in draw.");

    // 3.   Present the image to screen once it has signalled to finish rendering.
    VkPresentInfoKHR present_info = {};
    present_info.sType                  = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount     = 1;
    present_info.pWaitSemaphores        = &this->renders_finished[current_frame];
    present_info.swapchainCount         = 1;
    present_info.pSwapchains            = &this->swapchain;
    present_info.pImageIndices          = &image_index;         // Index of images in swapchain to present.

    result = vkQueuePresentKHR(this->graphics_queue, &present_info);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to present image to device.");

    // Set to the next frame.
    this->current_frame = (this->current_frame + 1) % max_frame_draws;

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
        this->create_render_pass();
        this->create_graphics_pipeline();
        this->create_framebuffers();
        this->create_command_pool();
        this->create_command_buffers();
        this->record_commands();
        this->create_synchronization();

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

    // Wait for queues to be cleaned up.
    vkDeviceWaitIdle(this->main_device.logical_device);

    this->destroy_synchronization();
    this->destroy_command_buffers();
    this->destroy_command_pool();
    this->destroy_framebuffers();
    this->destroy_graphics_pipeline();
    this->destroy_render_pass();
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
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        //VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
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

void VulkanRenderer::           
create_graphics_pipeline()
{

    // Get the shaders.
    auto vertex_shader_code = read_file("./shaders/vert.spv");
    auto fragment_shader_code = read_file("./shaders/frag.spv");

    // Build the shader modules.
    VkShaderModule vertex_shader_module = this->create_shader_module(vertex_shader_code);
    VkShaderModule fragment_shader_module = this->create_shader_module(fragment_shader_code);

    // Create the pipeline.
    VkPipelineShaderStageCreateInfo vertex_stage_create_info = {};
    vertex_stage_create_info.sType      = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage_create_info.stage      = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage_create_info.module     = vertex_shader_module;
    vertex_stage_create_info.pName      = "main";

    VkPipelineShaderStageCreateInfo fragment_stage_create_info = {};
    fragment_stage_create_info.sType      = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage_create_info.stage      = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage_create_info.module     = fragment_shader_module;
    fragment_stage_create_info.pName      = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_stage_create_info, fragment_stage_create_info };

    // Stage 1: Vertex Input Stage
    VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {};   
    vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_create_info.vertexBindingDescriptionCount      = 0;
    vertex_input_create_info.pVertexBindingDescriptions         = nullptr; // Data spacing & stride.
    vertex_input_create_info.vertexAttributeDescriptionCount    = 0;
    vertex_input_create_info.pVertexAttributeDescriptions       = nullptr; // Attribute vertex descriptions.

    // Input assembly.
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE; // Allows overriding strip topology...

    // Defining the viewport here.
    VkViewport viewport = {};
    viewport.x          = 0.0f;
    viewport.y          = 0.0f;
    viewport.width      = static_cast<float>(this->swapchain_extent.width);
    viewport.height     = static_cast<float>(this->swapchain_extent.height);
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;

    // Define the scissor struct.
    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = this->swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType            = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount    = 1;
    viewport_state_create_info.pViewports       = &viewport;
    viewport_state_create_info.scissorCount     = 1;
    viewport_state_create_info.pScissors        = &scissor;

    // Dynamic states... (Alterable states such as viewport size)
    /*
    std::vector<VkDynamicState> dynamic_state_enables;
    dynamic_state_enables.push_back(VK_DYNAMIC_STATE_VIEWPORT); // Dynamic viewport (resize in command buffer)
                                                                //      vkCmdSetViewport(cbuffer, 0, 1, &viewport)
    dynamic_state_enables.push_back(VK_DYNAMIC_STATE_SCISSOR);  // Dynamic scissor (resize in command buffer)
                                                                //      vkCmdSetScissor(cbuffer, 0, 1, &scissor.

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables.size());
    dynamic_state_create_info.pDynamicStates = dynamic_state_enables.data();
    */

    // Create rasterizer.
    VkPipelineRasterizationStateCreateInfo rasterizer_create_info = {};
    rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_create_info.depthClampEnable = VK_FALSE;         // Change near/far planes are clipped or clamped.
    rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;  // Throw away fragments... no render, just pre-rasterize
    rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;  // Points, lines, or fill (useful for debugging with line/points).
    rasterizer_create_info.lineWidth = 1.0f;                    // How thick, need extension for more than 1. (Enable WIDE_LINES)
    rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;    // Don't draw back of triangle.
    rasterizer_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE; // Counter-clock wise direction is backwards.
    rasterizer_create_info.depthBiasEnable = VK_FALSE;          // Add bias for shadows... (shadow acne?)

    // Multisampling (antialiasing)
    VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
    multisample_create_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_create_info.sampleShadingEnable     = VK_FALSE;                 // Disable multisampling.
    multisample_create_info.rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT;    // Number of samples to use per-frag.

    // Blending of fragments. (blending method)
    VkPipelineColorBlendAttachmentState color_state = {};
    color_state.colorWriteMask =    VK_COLOR_COMPONENT_R_BIT | 
                                    VK_COLOR_COMPONENT_G_BIT | 
                                    VK_COLOR_COMPONENT_B_BIT | 
                                    VK_COLOR_COMPONENT_A_BIT;
    color_state.blendEnable         = VK_TRUE;
    color_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_state.colorBlendOp        = VK_BLEND_OP_ADD;
    // Formula: (src * src_alpha) + (old * (1-src_alpha)
    color_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_state.alphaBlendOp        = VK_BLEND_OP_ADD;
    // Formula: (src_alpha * 1) + (old * 0)

    VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
    color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_create_info.logicOpEnable = VK_FALSE; // Alternative to calculations is logical ops.
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_state;

    // Pipeline layout.
    // TODO(Chris): Descriptor set layouts.
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount          = 0;
    pipeline_layout_create_info.pSetLayouts             = nullptr;
    pipeline_layout_create_info.pushConstantRangeCount  = 0;
    pipeline_layout_create_info.pPushConstantRanges     = nullptr;

    VkResult result = vkCreatePipelineLayout(this->main_device.logical_device, &pipeline_layout_create_info,
            nullptr, &this->pipeline_layout);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create pipeline layout.");

    // Depth stencil testing.
    // TODO(Chris): Set up depth stencil testing.

    // Create the graphics pipeline.
    VkGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount             = 2;
    pipeline_create_info.pStages                = shader_stages;
    pipeline_create_info.pVertexInputState      = &vertex_input_create_info;
    pipeline_create_info.pInputAssemblyState    = &input_assembly;
    pipeline_create_info.pViewportState         = &viewport_state_create_info;
    pipeline_create_info.pDynamicState          = nullptr;
    pipeline_create_info.pRasterizationState    = &rasterizer_create_info;
    pipeline_create_info.pMultisampleState      = &multisample_create_info;
    pipeline_create_info.pColorBlendState       = &color_blend_create_info;
    pipeline_create_info.pDepthStencilState     = nullptr;
    pipeline_create_info.layout                 = this->pipeline_layout;
    pipeline_create_info.renderPass             = this->render_pass;
    pipeline_create_info.subpass                = 0;

    // Pipeline derivatives, create multiple pipelines that derive from another for optimization.
    pipeline_create_info.basePipelineHandle     = VK_NULL_HANDLE;   // Handle.
    pipeline_create_info.basePipelineIndex      = -1;               // Or index.

    result = vkCreateGraphicsPipelines(this->main_device.logical_device, VK_NULL_HANDLE,
            1, &pipeline_create_info, nullptr, &this->graphics_pipeline);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create graphics pipeline.");

    // Clean up the shader modules.
    vkDestroyShaderModule(this->main_device.logical_device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(this->main_device.logical_device, vertex_shader_module, nullptr);

}

void VulkanRenderer::           
destroy_graphics_pipeline()
{

    vkDestroyPipeline(this->main_device.logical_device, this->graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(this->main_device.logical_device, this->pipeline_layout, nullptr);

}

VkShaderModule VulkanRenderer::         
create_shader_module(const std::vector<char> &code)
{

    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = code.size();
    shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(this->main_device.logical_device, &shader_module_create_info,
            nullptr, &module);
    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create shader module.");
    return std::move(module);

}

void VulkanRenderer::           
create_render_pass()
{

    // Color attachment of render pass.
    VkAttachmentDescription color_attachment = {};
    color_attachment.format     = this->swapchain_format;               // Format to use for attachment.
    color_attachment.samples    = VK_SAMPLE_COUNT_1_BIT;                // Number of samples for multisampling.
    color_attachment.loadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;          // Similar to glClear() (before rendering)
    color_attachment.storeOp    = VK_ATTACHMENT_STORE_OP_STORE;         // We want to push pixels. (after rendering)
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // Not using stencils.
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;     // Not using stencils.
    
    // Framebuffer data will be stored as image, but images can be given different
    // data layouts to give optimal use for certain operations.
    color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Image we need to change to...

    // Attachment reference which corresponds to the above attachment.
    VkAttachmentReference color_attachment_reference = {};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create the render subpass.
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_attachment_reference;

    // Need to determine when layout transitions occurs using subpass dependencies.
    std::array<VkSubpassDependency, 2> subpass_dependencies;   

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    //      After read bit is defined, synchronize before the the next stage's read/write bits for color attachment.
    subpass_dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpass_dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;

    subpass_dependencies[0].dstSubpass      = 0;
    subpass_dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = 0;

    // Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    subpass_dependencies[1].srcSubpass      = 0;
    subpass_dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    subpass_dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpass_dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    subpass_dependencies[1].dependencyFlags = 0;

    // Create the info for render pass.
    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments    = &color_attachment;
    render_pass_create_info.subpassCount    = 1;
    render_pass_create_info.pSubpasses      = &subpass;
    render_pass_create_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
    render_pass_create_info.pDependencies   = subpass_dependencies.data();

    VkResult result = vkCreateRenderPass(this->main_device.logical_device, &render_pass_create_info,
            nullptr, &this->render_pass);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create render pass.");

}

void VulkanRenderer::           
destroy_render_pass()
{

    vkDestroyRenderPass(this->main_device.logical_device, this->render_pass, nullptr);

}

void VulkanRenderer::            
create_framebuffers()
{

    this->swapchain_framebuffers.resize(this->swapchain_images.size());

    // Create framebuffer for each swapchain image.
    for (uint64_t i = 0; i < this->swapchain_framebuffers.size(); ++i)
    {

        // Need to line up in the render pass definitions.
        std::array<VkImageView, 1> attachments
        {
            this->swapchain_images[i].view,
        };

        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass      = this->render_pass;
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_create_info.pAttachments    = attachments.data();
        framebuffer_create_info.width           = this->swapchain_extent.width;
        framebuffer_create_info.height          = this->swapchain_extent.height;
        framebuffer_create_info.layers          = 1;

        VkResult result = vkCreateFramebuffer(this->main_device.logical_device,
                &framebuffer_create_info, nullptr, &this->swapchain_framebuffers[i]);

        if (result != VK_SUCCESS) throw std::runtime_error("Failed to create a framebuffer.");

    }

}

void VulkanRenderer::
destroy_framebuffers()
{

    for (auto framebuffer : this->swapchain_framebuffers)
    {

        vkDestroyFramebuffer(this->main_device.logical_device, framebuffer, nullptr);

    }

}

void VulkanRenderer::           
create_command_pool()
{

    QueueFamilyIndices indices = this->get_queue_families(this->main_device.physical_device);

    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType              = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex   = indices.graphics_family;

    // Create a graphics command queue family pool.
    VkResult result = vkCreateCommandPool(this->main_device.logical_device, &command_pool_create_info,
            nullptr, &this->graphics_command_pool);

    if (result != VK_SUCCESS) throw std::runtime_error("Failed to create graphics command pool.");

}

void VulkanRenderer::
destroy_command_pool()
{

    vkDestroyCommandPool(this->main_device.logical_device, this->graphics_command_pool, nullptr);

}

void VulkanRenderer::
create_command_buffers()
{

    this->command_buffers.resize(this->swapchain_framebuffers.size());

    VkCommandBufferAllocateInfo command_buffer_alloc_info = {};
    command_buffer_alloc_info.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_alloc_info.level         = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary is direct, secondary must be
                                                                               // called by another command queue.
    command_buffer_alloc_info.commandPool   = this->graphics_command_pool;
    command_buffer_alloc_info.commandBufferCount = static_cast<uint32_t>(this->command_buffers.size());

    VkResult result = vkAllocateCommandBuffers(this->main_device.logical_device, &command_buffer_alloc_info, 
            this->command_buffers.data());

    if (result != VK_SUCCESS) throw std::runtime_error("Unable to allocate command buffers.");

}

void VulkanRenderer::
destroy_command_buffers()
{

    // A do nothing routine.

}

void VulkanRenderer::
record_commands()
{

    VkCommandBufferBeginInfo buffer_begin_info = {};
    buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Information on how to begin render pass (required only for graphical.)
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass           = this->render_pass;
    render_pass_begin_info.renderArea.offset    = { 0, 0 };
    render_pass_begin_info.renderArea.extent    = this->swapchain_extent;

    // TODO(Chris): Depth attachment would also be added here.
    VkClearValue clear_values[]
    {
        { 0.1f, 0.105f, 0.1f, 1.0f },
    };

    render_pass_begin_info.pClearValues         = clear_values;
    render_pass_begin_info.clearValueCount      = 1;

    for (uint64_t i = 0; i < this->command_buffers.size(); ++i)
    {

        render_pass_begin_info.framebuffer = this->swapchain_framebuffers[i];

        // Start recording commands to the command buffers.
        VkResult result = vkBeginCommandBuffer(this->command_buffers[i], &buffer_begin_info);
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to start recording command buffer.");

        // ---------------------------------------------------------------------
        
        // Begin the render pass.
        vkCmdBeginRenderPass(this->command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline.
        vkCmdBindPipeline(this->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphics_pipeline);

        // Actually draw.
        vkCmdDraw(this->command_buffers[i], 3, 1, 0, 0);

        // End render pass.
        vkCmdEndRenderPass(this->command_buffers[i]);

        // ---------------------------------------------------------------------

        // End recording.
        result = vkEndCommandBuffer(this->command_buffers[i]);

    }


}

void VulkanRenderer::           
create_synchronization()
{

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    this->images_available.resize(max_frame_draws);
    this->renders_finished.resize(max_frame_draws);
    this->draw_fences.resize(max_frame_draws);

    for (uint64_t i = 0; i < max_frame_draws; ++i)
    {

        VkResult image_result = vkCreateSemaphore(this->main_device.logical_device,
                &semaphore_create_info, nullptr, &this->images_available[i]);
        VkResult render_result = vkCreateSemaphore(this->main_device.logical_device,
                &semaphore_create_info, nullptr, &this->renders_finished[i]);
        VkResult fence_result = vkCreateFence(this->main_device.logical_device,
                &fence_create_info, nullptr, &this->draw_fences[i]);

        if (image_result != VK_SUCCESS || render_result != VK_SUCCESS || fence_result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create semaphore/fence.");
        }

    }

}

void VulkanRenderer::
destroy_synchronization()
{

    for (uint64_t i = 0; i < max_frame_draws; ++i)
    {

        vkDestroySemaphore(this->main_device.logical_device, this->images_available[i], nullptr);
        vkDestroySemaphore(this->main_device.logical_device, this->renders_finished[i], nullptr);
        vkDestroyFence(this->main_device.logical_device, this->draw_fences[i], nullptr);

    }

}
