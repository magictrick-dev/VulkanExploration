#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

int
main(int argc, char ** argv)
{

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1600, 900, "Vulkan Probe", nullptr, nullptr);

    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    printf("Extensions: %lu\n", extension_count);

    glm::mat4 testMatrix(1.0f);
    glm::vec4 testVector(1.0f);

    glm::vec4 result = testMatrix * testVector;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;

}
