#ifndef SOURCE_VKRENDERER_HPP
#define SOURCE_VKRENDERER_HPP
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <utilities.hpp>

#define VK_RENDERER_INIT_SUCCESS        0
#define VK_RENDERER_INIT_NULL_WINDOW   -1
#define VK_RENDERER_INIT_CREATE_FAIL   -2

class VulkanRenderer
{

    public:
                        VulkanRenderer();
                       ~VulkanRenderer();

        int32_t         initialize(GLFWwindow *window);
        void            deinitialize();

    private:
        void            create_instance();
        void            create_logical_device();
        void            destroy_instance();
        void            destroy_logical_device();

        void            get_physical_device();

    private:
        bool            check_instance_extension_support(std::vector<const char*>& extensions);
        bool            check_device_suitability(VkPhysicalDevice &device);
        bool            check_validation_support();

        QueueFamilyIndices      get_queue_families(VkPhysicalDevice &device) const;

    private:
        GLFWwindow *window;

        VkInstance instance;
        VkQueue graphics_queue;

        struct
        {
            VkPhysicalDevice    physical_device;
            VkDevice            logical_device;
        } main_device;

        std::vector<const char*> validation_layers;

};

#endif
