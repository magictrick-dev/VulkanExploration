#ifndef SOURCE_VKRENDERER_HPP
#define SOURCE_VKRENDERER_HPP
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
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
        void            create_surface();
        void            create_swapchain();
        void            create_render_pass();
        void            create_graphics_pipeline();

        void            destroy_instance();
        void            destroy_logical_device();
        void            destroy_surface();
        void            destroy_swapchain();
        void            destroy_render_pass();
        void            destroy_graphics_pipeline();

        void            get_physical_device();

    private:
        bool            check_instance_extension_support(std::vector<const char*>& extensions);
        bool            check_device_suitability(VkPhysicalDevice &device);
        bool            check_device_extension_support(VkPhysicalDevice &device);
        bool            check_validation_support();

        QueueFamilyIndices      get_queue_families(VkPhysicalDevice &device) const;
        SwapChainDetails        get_swapchain_details(VkPhysicalDevice &device) const;

        VkSurfaceFormatKHR      choose_optimal_surface_format(std::vector<VkSurfaceFormatKHR> &formats);
        VkPresentModeKHR        choose_optimal_presentation_mode(std::vector<VkPresentModeKHR> &modes);
        VkExtent2D              choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities);

        VkShaderModule          create_shader_module(const std::vector<char> &code);

    private:
        GLFWwindow     *window;

        VkInstance      instance;
        VkQueue         graphics_queue;
        VkQueue         presentation_queue;
        VkSurfaceKHR    surface;
        VkSwapchainKHR  swapchain;
        VkPipeline      graphics_pipeline;

        VkPipelineLayout    pipeline_layout;
        VkRenderPass        render_pass;

        VkFormat        swapchain_format;
        VkExtent2D      swapchain_extent;
        std::vector<SwapChainImage> swapchain_images;
        VkImageView     create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);

        struct
        {
            VkPhysicalDevice    physical_device;
            VkDevice            logical_device;
        } main_device;



        std::vector<const char*> validation_layers;
        VkDebugUtilsMessengerEXT debug_messenger;

};

#endif
