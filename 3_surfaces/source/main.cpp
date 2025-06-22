#include <iostream>
#include <stdexcept>
#include <vector>
#include <VkRenderer.hpp>

static GLFWwindow *window;
static VulkanRenderer vulkan_renderer;
static bool init_window(const char *window_name = "Vulkan Probe", const int32_t width = 800, const int32_t height = 600);

int
main(int argc, char ** argv)
{

    // Create the window.
    if (!init_window()) return -1;

    // Initialize vulkan renderer.
    int32_t result = vulkan_renderer.initialize(window);
    if (result != VK_RENDERER_INIT_SUCCESS) return -1;

    // Main runtime loop.
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    vulkan_renderer.deinitialize();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;

}

// --- Helpers -----------------------------------------------------------------
//
// Various helper functions to provide front-end functionality not necessarily
// directly related with probing the Vulkan API.
//

static bool 
init_window(const char *window_name, const int32_t width, const int32_t height)
{

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, window_name, nullptr, nullptr);
    return (window != nullptr);

}
