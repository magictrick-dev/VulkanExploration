#include <VkRenderer.hpp>

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
        this->get_physical_device();

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

    this->destroy_instance();

}

void VulkanRenderer::
create_instance()
{

    // Application information for a Vulkan instance.
    // NOTE(Chris): Mostly for developer convenience.
    VkApplicationInfo application_info = {};
    application_info.sType                  = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName       = "Vulkan Probe";
    application_info.applicationVersion     = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName            = "No Engine";
    application_info.engineVersion          = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion             = VK_API_VERSION_1_4;

    // Creation information for a Vulkan instance.
    VkInstanceCreateInfo create_info = {};
    create_info.sType                       = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo            = &application_info;

    // Generate list to hold instance extensions.
    std::vector<const char*> instance_extensions;

    uint32_t glfw_extension_count = 0; // Basically what extensions GLFW requires.
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    for (uint64_t i = 0; i < glfw_extension_count; ++i)
    {

        instance_extensions.push_back(glfw_extensions[i]);

    }

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
    return queue_family_indices_valid(&indices);

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
            break;

        }

        i++;

    }

    return std::move(indices);


}

