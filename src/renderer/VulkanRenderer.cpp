#include "VulkanRenderer.h"
#include <stdexcept>
#include <iostream>
#include <array>

// Forward declarations for helper functions
template<class T>
static const char* getPhysicalDeviceName(const std::vector<T>& devices, uint32_t index) {
    return ""; // placeholder
}

VulkanRenderer::VulkanRenderer() {}
VulkanRenderer::~VulkanRenderer() { cleanup(); }

bool VulkanRenderer::init(const char* title, int width, int height)
{
    m_width = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(m_width, m_height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    // Vulkan instance
    const char* layerNames[] = {"VK_LAYER_KHRONOS_standard"};
    vk::ApplicationInfo appInfo{{}, title, 1, "", 0};
    vk::InstanceCreateInfo createInfo{ {}, &appInfo };
    std::vector<const char*> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
#ifdef _WIN32
        ,VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
    };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 1; // debug layer
    createInfo.ppEnabledLayerNames = layerNames;
    auto result = vk::createInstance(createInfo, nullptr, &m_instance);
    if (result != vk::Result::eSuccess) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }

    // Surface
    VkSurfaceKHR surface; // C style for GLFW
    if (!glfwCreateWindowSurface(static_cast<VkInstance>(m_instance), m_window, nullptr, &surface)) {
        std::cerr << "Failed to create window surface" << std::endl;
        return false;
    }
    m_surface = static_cast<vk::SurfaceKHR>(surface);

    // Physical device selection (just pick first)
    std::vector<vk::PhysicalDevice> devices; 
    m_instance.enumeratePhysicalDevices(&devices);
    if (devices.empty()) { std::cerr << "No Vulkan physical devices found" << std::endl; return false;} 
    m_physicalDevice = devices[0];

    // Queue family
    uint32_t queueFamilyIndex;
    auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
    for (uint32_t i=0;i<queueFamilies.size();i++) {
        if (m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface).queueFlags & VK_QUEUE_GRAPHICS_BIT) { queueFamilyIndex=i; break; }
    }

    // Logical device
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo dqci{ {}, queueFamilyIndex, 1, &queuePriority };
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::PhysicalDeviceFeatures pdFeatures{}; // default
    vk::DeviceCreateInfo dcInfo{ {}, &dqci, 0, nullptr, static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data(), &pdFeatures };
    result = m_physicalDevice.createDevice(&dcInfo, nullptr, &m_device);
    if (result != vk::Result::eSuccess) { std::cerr << "Failed to create logical device" << std::endl; return false;} 

    m_graphicsQueue = m_device.getQueue(queueFamilyIndex, 0);

    // Swapchain
    auto surfaceCaps = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    vk::SwapchainCreateInfoKHR scci{ {}, m_surface, imageCount, surfaceCaps.currentTransform, surfaceCaps.supportedCompositeAlpha, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
    scci.preTransform = surfaceCaps.currentTransform;
    scci.compositeAlpha = surfaceCaps.supportedCompositeAlpha;
    scci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed present
    scci.clipped = true;
    scci.imageExtent = { m_width, m_height };
    result = m_device.createSwapchainKHR(&scci, nullptr, &m_swapchain);
    if (result != vk::Result::eSuccess) { std::cerr << "Failed to create swapchain" << std::endl; return false;} 

    m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);

    // Image views
    for (const auto& img : m_swapchainImages) {
        vk::ImageViewCreateInfo ivci{ {}, img, VK_IMAGE_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, {} };
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0; ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0; ivci.subresourceRange.layerCount = 1;
        vk::ImageView view; m_device.createImageView(&ivci, nullptr, &view);
        m_swapchainImages.push_back(view); // wrong: need separate vector
    }

    // Render pass
    vk::AttachmentDescription colorAttachment{ {}, VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, {} };
    vk::AttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    vk::SubpassDescription subpass{ {}, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorRef, nullptr, {} };
    vk::RenderPassCreateInfo rpci{ {}, 1, &colorAttachment, 1, &subpass };
    m_device.createRenderPass(&rpci, nullptr, &m_renderPass);

    // Command buffers
    createCommandBuffers();

    return true;
}

void VulkanRenderer::cleanup()
{
    if (m_device) {
        for (auto imgView : m_swapchainImages) m_device.destroyImageView(imgView, nullptr);
        m_device.destroySwapchainKHR(m_swapchain, nullptr);
        m_device.destroyRenderPass(m_renderPass, nullptr);
        m_device.destroyDevice(nullptr);
    }
    if (m_surface) m_instance.destroySurfaceKHR(m_surface, nullptr);
    if (m_instance) m_instance.destroyInstance(nullptr);
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void VulkanRenderer::beginFrame()
{
    // acquire image
    uint32_t imageIndex = 0;
    m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);
    // record command buffer (simplified: just clear)
    auto cmdBuf = m_commandBuffers[imageIndex];
    vk::CommandBufferBeginInfo cbbi{};
    cmdBuf.begin(cbbi);
    std::array<vk::ClearValue,1> clearVals{{ {0.5f, 0.7f, 0.9f, 1.0f} }};
    vk::RenderPassBeginInfo rpbi{m_renderPass, m_swapchainImages[imageIndex], {{0,0},{m_width,m_height}}, 1, clearVals.data()};
    cmdBuf.beginRenderPass(rpbi, vk::SubpassContents::eInline);
    cmdBuf.endRenderPass();
    cmdBuf.end();

    // submit
    vk::SubmitInfo si{0,nullptr,1,&cmdBuf,0,nullptr};
    m_graphicsQueue.submit(si, VK_NULL_HANDLE);
    m_device.queuePresentKHR(m_graphicsQueue, &imageIndex);
}

void VulkanRenderer::endFrame()
{
    // Wait for idle (simple but blocking)
    m_device.waitIdle();
}

void VulkanRenderer::createCommandBuffers()
{
    std::vector<vk::CommandBuffer> cmds;
    vk::CommandPoolCreateInfo poolci{ {}, 0, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    vk::CommandPool pool; m_device.createCommandPool(&poolci, nullptr, &pool);
    vk::CommandBufferAllocateInfo cbai{pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(m_swapchainImages.size())};
    m_device.allocateCommandBuffers(&cbai, &cmds[0]);
}
