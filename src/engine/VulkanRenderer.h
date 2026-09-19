#pragma once
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <vector>

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool init(const char* title, int width, int height);
    void cleanup();

    bool shouldClose() const { return glfwWindowShouldClose(m_window); }
    void pollEvents() { glfwPollEvents(); }

    void beginFrame();
    void endFrame();

private:
    // Window and swapchain
    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 0, m_height = 0;

    // Vulkan core objects
    vk::Instance m_instance;
    vk::SurfaceKHR m_surface;
    vk::Device m_device;
    vk::PhysicalDevice m_physicalDevice;
    vk::Queue m_graphicsQueue;
    vk::RenderPass m_renderPass;
    vk::SwapchainKHR m_swapchain;
    std::vector<vk::ImageView> m_swapchainImages;

    // Command buffers
    std::vector<vk::CommandBuffer> m_commandBuffers;
};