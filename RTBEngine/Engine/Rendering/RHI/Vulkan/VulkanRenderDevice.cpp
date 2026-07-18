#include "VulkanRenderDevice.h"
#include "../../../Core/Logger.h"
#include <SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <string>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            namespace {

                constexpr std::array<const char*, 1> kValidationLayers = { "VK_LAYER_KHRONOS_validation" };
                constexpr std::array<const char*, 1> kDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef _DEBUG
                constexpr bool kEnableValidationLayers = true;
#else
                constexpr bool kEnableValidationLayers = false;
#endif

                VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                    void* /*userData*/)
                {
                    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                        RTB_ERROR(std::string("[Vulkan] ") + callbackData->pMessage);
                    }
                    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                        RTB_WARN(std::string("[Vulkan] ") + callbackData->pMessage);
                    }
                    return VK_FALSE;
                }

                VkResult CreateDebugUtilsMessengerEXT(VkInstance vkInstance,
                    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
                    const VkAllocationCallbacks* allocator,
                    VkDebugUtilsMessengerEXT* messenger)
                {
                    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                        vkGetInstanceProcAddr(vkInstance, "vkCreateDebugUtilsMessengerEXT"));
                    if (func) {
                        return func(vkInstance, createInfo, allocator, messenger);
                    }
                    return VK_ERROR_EXTENSION_NOT_PRESENT;
                }

                void DestroyDebugUtilsMessengerEXT(VkInstance vkInstance,
                    VkDebugUtilsMessengerEXT messenger,
                    const VkAllocationCallbacks* allocator)
                {
                    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                        vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugUtilsMessengerEXT"));
                    if (func) {
                        func(vkInstance, messenger, allocator);
                    }
                }

                bool CheckDeviceExtensionSupport(VkPhysicalDevice candidate)
                {
                    std::uint32_t count = 0;
                    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr);
                    std::vector<VkExtensionProperties> available(count);
                    vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, available.data());

                    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
                    for (const auto& ext : available) {
                        required.erase(ext.extensionName);
                    }
                    return required.empty();
                }

            }

            VulkanRenderDevice::~VulkanRenderDevice()
            {
                Shutdown();
            }

            bool VulkanRenderDevice::Initialize(SDL_Window* sdlWindow, bool vSync)
            {
                if (initialized) {
                    return true;
                }

                if (!sdlWindow) {
                    RTB_ERROR("VulkanRenderDevice::Initialize - null SDL_Window");
                    return false;
                }

                window = sdlWindow;
                vSyncEnabled = vSync;

                if (!CreateInstance()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create VkInstance");
                    return false;
                }

                SetupDebugMessenger();

                if (!CreateSurface()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create VkSurfaceKHR");
                    return false;
                }

                if (!PickPhysicalDevice()) {
                    RTB_ERROR("VulkanRenderDevice: failed to find a suitable physical device");
                    return false;
                }

                if (!CreateLogicalDevice()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create VkDevice");
                    return false;
                }

                if (!CreateSwapchain() || swapchain == VK_NULL_HANDLE) {
                    RTB_ERROR("VulkanRenderDevice: failed to create swapchain");
                    return false;
                }

                if (!CreateSwapchainImageSemaphores()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create per-image present semaphores");
                    return false;
                }

                if (!CreateImageViews()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create swapchain image views");
                    return false;
                }

                if (!CreateRenderPass()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create render pass");
                    return false;
                }

                if (!CreateFramebuffers()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create framebuffers");
                    return false;
                }

                if (!CreateCommandPool()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create command pool");
                    return false;
                }

                if (!CreateCommandBuffers()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create command buffers");
                    return false;
                }

                if (!CreateSyncObjects()) {
                    RTB_ERROR("VulkanRenderDevice: failed to create synchronization objects");
                    return false;
                }

                initialized = true;
                RTB_INFO("VulkanRenderDevice initialized");
                return true;
            }

            void VulkanRenderDevice::Shutdown()
            {
                if (device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(device);
                }

                for (VkFence fence : inFlightFences) {
                    if (fence != VK_NULL_HANDLE) {
                        vkDestroyFence(device, fence, nullptr);
                    }
                }
                inFlightFences.clear();
                imagesInFlight.clear();

                for (VkSemaphore semaphore : imageAvailableSemaphores) {
                    if (semaphore != VK_NULL_HANDLE) {
                        vkDestroySemaphore(device, semaphore, nullptr);
                    }
                }
                imageAvailableSemaphores.clear();

                if (commandPool != VK_NULL_HANDLE) {
                    // Destroying the pool also frees the command buffers allocated from it.
                    vkDestroyCommandPool(device, commandPool, nullptr);
                    commandPool = VK_NULL_HANDLE;
                }
                commandBuffers.clear();

                CleanupSwapchain();

                if (renderPass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(device, renderPass, nullptr);
                    renderPass = VK_NULL_HANDLE;
                }

                if (device != VK_NULL_HANDLE) {
                    vkDestroyDevice(device, nullptr);
                    device = VK_NULL_HANDLE;
                }

                if (surface != VK_NULL_HANDLE) {
                    vkDestroySurfaceKHR(instance, surface, nullptr);
                    surface = VK_NULL_HANDLE;
                }

                if (debugMessenger != VK_NULL_HANDLE) {
                    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
                    debugMessenger = VK_NULL_HANDLE;
                }

                if (instance != VK_NULL_HANDLE) {
                    vkDestroyInstance(instance, nullptr);
                    instance = VK_NULL_HANDLE;
                }

                physicalDevice = VK_NULL_HANDLE;
                graphicsQueue = VK_NULL_HANDLE;
                presentQueue = VK_NULL_HANDLE;
                window = nullptr;
                initialized = false;
            }

            void VulkanRenderDevice::MakeCurrent()
            {
                // No-op: Vulkan has no notion of a "current context" per thread like GL does.
            }

            void VulkanRenderDevice::Present()
            {
                if (!initialized) {
                    return;
                }

                if (swapchain == VK_NULL_HANDLE) {
                    RecreateSwapchain();
                    return;
                }

                const VkFence currentFence = inFlightFences[currentFrame];
                vkWaitForFences(device, 1, &currentFence, VK_TRUE, UINT64_MAX);

                std::uint32_t imageIndex = 0;
                const VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                    imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

                if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                    RecreateSwapchain();
                    return;
                }
                if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                    RTB_ERROR("VulkanRenderDevice: vkAcquireNextImageKHR failed");
                    return;
                }

                // Wait if a previous frame is still using this swapchain image.
                if (imageIndex < imagesInFlight.size() && imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                    vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
                }
                imagesInFlight[imageIndex] = currentFence;

                vkResetFences(device, 1, &currentFence);

                const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
                vkResetCommandBuffer(commandBuffer, 0);

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkBeginCommandBuffer failed");
                    return;
                }

                VkClearValue clearValue{};
                clearValue.color = { { clearColor[0], clearColor[1], clearColor[2], clearColor[3] } };

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderPass;
                renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchainExtent;
                renderPassInfo.clearValueCount = 1;
                renderPassInfo.pClearValues = &clearValue;

                vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
                // MVP: only clears the framebuffer. Actual draw calls will be recorded here
                // once the Vulkan resource pipeline (shaders/buffers/pipelines) is implemented.
                vkCmdEndRenderPass(commandBuffer);

                if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkEndCommandBuffer failed");
                    return;
                }

                // Present-wait semaphore must be per swapchain image, not per frame-in-flight:
                // otherwise present may still be using the semaphore when we signal it again.
                const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
                const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = signalSemaphores;

                if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFence) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkQueueSubmit failed");
                    return;
                }

                const VkSwapchainKHR swapchains[] = { swapchain };
                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = signalSemaphores;
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = swapchains;
                presentInfo.pImageIndices = &imageIndex;

                const VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);
                if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
                    RecreateSwapchain();
                }
                else if (presentResult != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkQueuePresentKHR failed");
                }

                currentFrame = (currentFrame + 1) % static_cast<std::size_t>(kMaxFramesInFlight);
            }

            void VulkanRenderDevice::SetVSync(bool enabled)
            {
                if (vSyncEnabled == enabled) {
                    return;
                }

                vSyncEnabled = enabled;
                if (initialized) {
                    RecreateSwapchain();
                }
            }

            void VulkanRenderDevice::SetViewport(int x, int y, int width, int height)
            {
                // Stored for future dynamic-viewport pipelines; the MVP render pass covers
                // the full swapchain extent.
                pendingViewport[0] = x;
                pendingViewport[1] = y;
                pendingViewport[2] = width;
                pendingViewport[3] = height;
            }

            void VulkanRenderDevice::SetClearColor(float r, float g, float b, float a)
            {
                clearColor[0] = r;
                clearColor[1] = g;
                clearColor[2] = b;
                clearColor[3] = a;
            }

            void VulkanRenderDevice::Clear(ClearMask mask)
            {
                // The MVP render pass always clears the color attachment on load; this just
                // records what was requested for parity with the OpenGL backend.
                pendingClearMask = pendingClearMask | mask;
            }

            void VulkanRenderDevice::SetDepthTest(bool /*enabled*/) {}
            void VulkanRenderDevice::SetDepthFunc(DepthFunc /*func*/) {}
            void VulkanRenderDevice::SetDepthWrite(bool /*enabled*/) {}
            void VulkanRenderDevice::SetCullFace(bool /*enabled*/) {}
            void VulkanRenderDevice::SetBlend(bool /*enabled*/) {}
            void VulkanRenderDevice::SetBlendFuncSeparate(int /*srcRGB*/, int /*dstRGB*/, int /*srcAlpha*/, int /*dstAlpha*/) {}
            void VulkanRenderDevice::SetColorMask(bool /*red*/, bool /*green*/, bool /*blue*/, bool /*alpha*/) {}

            GpuId VulkanRenderDevice::CreateShaderProgram(const std::string& /*vertexSource*/,
                                                          const std::string& /*fragmentSource*/)
            {
                return nextId++;
            }
            void VulkanRenderDevice::DestroyShaderProgram(GpuId /*program*/) {}
            void VulkanRenderDevice::BindShaderProgram(GpuId /*program*/) {}
            int VulkanRenderDevice::GetUniformLocation(GpuId /*program*/, const char* /*name*/) { return -1; }
            void VulkanRenderDevice::SetUniformBool(int /*location*/, bool /*value*/) {}
            void VulkanRenderDevice::SetUniformInt(int /*location*/, int /*value*/) {}
            void VulkanRenderDevice::SetUniformFloat(int /*location*/, float /*value*/) {}
            void VulkanRenderDevice::SetUniformVec2(int /*location*/, float /*x*/, float /*y*/) {}
            void VulkanRenderDevice::SetUniformVec3(int /*location*/, float /*x*/, float /*y*/, float /*z*/) {}
            void VulkanRenderDevice::SetUniformVec4(int /*location*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/) {}
            void VulkanRenderDevice::SetUniformMat4(int /*location*/, const float* /*matrix4x4*/) {}
            void VulkanRenderDevice::BindUniformBlock(GpuId /*program*/, const char* /*blockName*/, unsigned int /*bindingPoint*/) {}

            GpuId VulkanRenderDevice::CreateBuffer() { return nextId++; }
            void VulkanRenderDevice::DestroyBuffer(GpuId /*buffer*/) {}
            void VulkanRenderDevice::SetUniformBufferData(GpuId /*buffer*/, const void* /*data*/, std::size_t /*size*/, BufferUsage /*usage*/) {}
            void VulkanRenderDevice::UpdateUniformBufferData(GpuId /*buffer*/, const void* /*data*/, std::size_t /*size*/) {}
            void VulkanRenderDevice::BindUniformBufferBase(GpuId /*buffer*/, unsigned int /*bindingPoint*/) {}

            GpuId VulkanRenderDevice::CreateTexture2D() { return nextId++; }
            void VulkanRenderDevice::DestroyTexture(GpuId /*texture*/) {}
            void VulkanRenderDevice::SetTexture2DData(GpuId /*texture*/, TextureFormat /*format*/, int /*width*/, int /*height*/,
                                                      const void* /*pixels*/, bool /*generateMipmaps*/) {}
            void VulkanRenderDevice::SetTexture2DFilter(GpuId /*texture*/, TextureFilter /*minFilter*/, TextureFilter /*magFilter*/) {}
            void VulkanRenderDevice::SetTexture2DWrap(GpuId /*texture*/, TextureWrap /*wrapS*/, TextureWrap /*wrapT*/) {}
            void VulkanRenderDevice::SetTexture2DDepthShadowParams(GpuId /*texture*/) {}
            void VulkanRenderDevice::BindTexture2D(GpuId /*texture*/, unsigned int /*slot*/) {}
            void VulkanRenderDevice::UnbindTexture2D() {}

            GpuId VulkanRenderDevice::CreateCubemap() { return nextId++; }
            void VulkanRenderDevice::SetCubemapFace(GpuId /*cubemap*/, int /*faceIndex*/, TextureFormat /*format*/,
                                                    int /*width*/, int /*height*/, const void* /*pixels*/) {}
            void VulkanRenderDevice::SetCubemapFilterWrap(GpuId /*cubemap*/) {}
            void VulkanRenderDevice::BindCubemap(GpuId /*cubemap*/, unsigned int /*slot*/) {}

            GpuId VulkanRenderDevice::CreateFramebuffer() { return nextId++; }
            void VulkanRenderDevice::DestroyFramebuffer(GpuId /*framebuffer*/) {}
            void VulkanRenderDevice::BindFramebuffer(GpuId /*framebuffer*/) {}
            void VulkanRenderDevice::UnbindFramebuffer() {}
            void VulkanRenderDevice::AttachFramebufferColorTexture(GpuId /*framebuffer*/, GpuId /*texture*/) {}
            void VulkanRenderDevice::AttachFramebufferDepthTexture(GpuId /*framebuffer*/, GpuId /*texture*/) {}
            void VulkanRenderDevice::SetFramebufferDrawReadNone() {}
            bool VulkanRenderDevice::IsFramebufferComplete() const { return true; }
            GpuId VulkanRenderDevice::CreateColorTextureForFramebuffer(int /*width*/, int /*height*/) { return nextId++; }
            GpuId VulkanRenderDevice::CreateDepthTextureForFramebuffer(int /*width*/, int /*height*/) { return nextId++; }

            GpuId VulkanRenderDevice::CreateVertexArray() { return nextId++; }
            void VulkanRenderDevice::DestroyVertexArray(GpuId /*vao*/) {}
            void VulkanRenderDevice::BindVertexArray(GpuId /*vao*/) {}
            void VulkanRenderDevice::UnbindVertexArray() {}
            void VulkanRenderDevice::SetArrayBufferData(GpuId /*buffer*/, const void* /*data*/, std::size_t /*size*/, BufferUsage /*usage*/) {}
            void VulkanRenderDevice::SetElementBufferData(GpuId /*buffer*/, const void* /*data*/, std::size_t /*size*/, BufferUsage /*usage*/) {}
            void VulkanRenderDevice::BindArrayBuffer(GpuId /*buffer*/) {}
            void VulkanRenderDevice::BindElementBuffer(GpuId /*buffer*/) {}
            void VulkanRenderDevice::EnableVertexAttribFloat(unsigned int /*location*/, int /*components*/, int /*stride*/, std::size_t /*offset*/) {}
            void VulkanRenderDevice::EnableVertexAttribInt(unsigned int /*location*/, int /*components*/, int /*stride*/, std::size_t /*offset*/) {}
            void VulkanRenderDevice::SetVertexAttribDivisor(unsigned int /*location*/, unsigned int /*divisor*/) {}
            void VulkanRenderDevice::DrawIndexed(PrimitiveTopology /*topology*/, int /*indexCount*/, IndexType /*indexType*/) {}
            void VulkanRenderDevice::DrawIndexedInstanced(PrimitiveTopology /*topology*/, int /*indexCount*/, IndexType /*indexType*/, int /*instanceCount*/) {}
            void VulkanRenderDevice::DrawArrays(PrimitiveTopology /*topology*/, int /*first*/, int /*count*/) {}
            void VulkanRenderDevice::DrawArraysInstanced(PrimitiveTopology /*topology*/, int /*first*/, int /*count*/, int /*instanceCount*/) {}

            unsigned int VulkanRenderDevice::GetNativeTextureIdForImGui(GpuId /*texture*/) const { return 0; }

            std::vector<const char*> VulkanRenderDevice::GetRequiredInstanceExtensions()
            {
                unsigned int count = 0;
                if (SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr) != SDL_TRUE) {
                    RTB_ERROR(std::string("VulkanRenderDevice: SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
                    return {};
                }

                std::vector<const char*> extensions(count);
                SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data());

                if (kEnableValidationLayers) {
                    std::uint32_t availableCount = 0;
                    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);
                    std::vector<VkExtensionProperties> available(availableCount);
                    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());

                    for (const auto& ext : available) {
                        if (std::strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                            debugUtilsAvailable = true;
                            break;
                        }
                    }

                    if (debugUtilsAvailable) {
                        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    }
                }

                return extensions;
            }

            bool VulkanRenderDevice::CheckValidationLayerSupport() const
            {
                std::uint32_t layerCount = 0;
                vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
                std::vector<VkLayerProperties> availableLayers(layerCount);
                vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

                for (const char* layerName : kValidationLayers) {
                    bool found = false;
                    for (const auto& layerProps : availableLayers) {
                        if (std::strcmp(layerName, layerProps.layerName) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateInstance()
            {
                const bool useValidation = kEnableValidationLayers && CheckValidationLayerSupport();
                if (kEnableValidationLayers && !useValidation) {
                    RTB_WARN("VulkanRenderDevice: validation layers requested but not available; continuing without them");
                }

                VkApplicationInfo appInfo{};
                appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                appInfo.pApplicationName = "RTBEngine Application";
                appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
                appInfo.pEngineName = "RTBEngine";
                appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
                appInfo.apiVersion = VK_API_VERSION_1_1;

                const std::vector<const char*> extensions = GetRequiredInstanceExtensions();

                VkInstanceCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                createInfo.pApplicationInfo = &appInfo;
                createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
                createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

                if (useValidation) {
                    createInfo.enabledLayerCount = static_cast<std::uint32_t>(kValidationLayers.size());
                    createInfo.ppEnabledLayerNames = kValidationLayers.data();
                }
                else {
                    createInfo.enabledLayerCount = 0;
                    createInfo.ppEnabledLayerNames = nullptr;
                }

                const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
                if (result != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateInstance failed with VkResult " + std::to_string(static_cast<int>(result)));
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::SetupDebugMessenger()
            {
                if (!kEnableValidationLayers || !debugUtilsAvailable) {
                    return true;
                }

                VkDebugUtilsMessengerCreateInfoEXT createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                createInfo.pfnUserCallback = DebugCallback;

                if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
                    RTB_WARN("VulkanRenderDevice: failed to set up debug messenger; continuing without it");
                    debugMessenger = VK_NULL_HANDLE;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateSurface()
            {
                if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE) {
                    RTB_ERROR(std::string("VulkanRenderDevice: SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
                    return false;
                }
                return true;
            }

            VulkanRenderDevice::QueueFamilyIndices VulkanRenderDevice::FindQueueFamilies(VkPhysicalDevice candidate) const
            {
                QueueFamilyIndices indices;

                std::uint32_t count = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
                std::vector<VkQueueFamilyProperties> families(count);
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());

                for (std::uint32_t i = 0; i < count; ++i) {
                    if (!indices.hasGraphics && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                        indices.graphicsFamily = i;
                        indices.hasGraphics = true;
                    }

                    VkBool32 presentSupport = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport);
                    if (!indices.hasPresent && presentSupport) {
                        indices.presentFamily = i;
                        indices.hasPresent = true;
                    }

                    if (indices.IsComplete()) {
                        break;
                    }
                }

                return indices;
            }

            VulkanRenderDevice::SwapChainSupportDetails VulkanRenderDevice::QuerySwapChainSupport(VkPhysicalDevice candidate) const
            {
                SwapChainSupportDetails details;
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &details.capabilities);

                std::uint32_t formatCount = 0;
                vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, nullptr);
                if (formatCount != 0) {
                    details.formats.resize(formatCount);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, details.formats.data());
                }

                std::uint32_t presentModeCount = 0;
                vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &presentModeCount, nullptr);
                if (presentModeCount != 0) {
                    details.presentModes.resize(presentModeCount);
                    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &presentModeCount, details.presentModes.data());
                }

                return details;
            }

            bool VulkanRenderDevice::IsDeviceSuitable(VkPhysicalDevice candidate) const
            {
                if (!FindQueueFamilies(candidate).IsComplete()) {
                    return false;
                }

                if (!CheckDeviceExtensionSupport(candidate)) {
                    return false;
                }

                const SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(candidate);
                return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            bool VulkanRenderDevice::PickPhysicalDevice()
            {
                std::uint32_t deviceCount = 0;
                vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
                if (deviceCount == 0) {
                    RTB_ERROR("VulkanRenderDevice: no Vulkan-capable physical devices found");
                    return false;
                }

                std::vector<VkPhysicalDevice> devices(deviceCount);
                vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

                VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
                int bestScore = -1;

                for (VkPhysicalDevice candidate : devices) {
                    if (!IsDeviceSuitable(candidate)) {
                        continue;
                    }

                    VkPhysicalDeviceProperties props{};
                    vkGetPhysicalDeviceProperties(candidate, &props);

                    int score = 1;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                        score = 100;
                    }
                    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                        score = 50;
                    }

                    if (score > bestScore) {
                        bestScore = score;
                        bestDevice = candidate;
                    }
                }

                if (bestDevice == VK_NULL_HANDLE) {
                    RTB_ERROR("VulkanRenderDevice: no suitable physical device found (needs graphics+present queues and swapchain support)");
                    return false;
                }

                physicalDevice = bestDevice;

                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(physicalDevice, &props);
                RTB_INFO(std::string("VulkanRenderDevice: selected physical device '") + props.deviceName + "'");

                return true;
            }

            bool VulkanRenderDevice::CreateLogicalDevice()
            {
                const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
                graphicsQueueFamily = indices.graphicsFamily;
                presentQueueFamily = indices.presentFamily;

                const std::set<std::uint32_t> uniqueQueueFamilies = { graphicsQueueFamily, presentQueueFamily };
                std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
                const float queuePriority = 1.0f;
                for (std::uint32_t family : uniqueQueueFamilies) {
                    VkDeviceQueueCreateInfo queueCreateInfo{};
                    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    queueCreateInfo.queueFamilyIndex = family;
                    queueCreateInfo.queueCount = 1;
                    queueCreateInfo.pQueuePriorities = &queuePriority;
                    queueCreateInfos.push_back(queueCreateInfo);
                }

                VkPhysicalDeviceFeatures deviceFeatures{};

                VkDeviceCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
                createInfo.pQueueCreateInfos = queueCreateInfos.data();
                createInfo.pEnabledFeatures = &deviceFeatures;
                createInfo.enabledExtensionCount = static_cast<std::uint32_t>(kDeviceExtensions.size());
                createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
                // Device layers are legacy/invalid on modern Vulkan; only instance layers are used.
                createInfo.enabledLayerCount = 0;
                createInfo.ppEnabledLayerNames = nullptr;

                if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateDevice failed");
                    return false;
                }

                vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
                vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
                return true;
            }

            VkSurfaceFormatKHR VulkanRenderDevice::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
            {
                for (const auto& format : formats) {
                    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        return format;
                    }
                }
                for (const auto& format : formats) {
                    if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
                        return format;
                    }
                }
                if (!formats.empty()) {
                    return formats[0];
                }
                return VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
            }

            VkPresentModeKHR VulkanRenderDevice::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const
            {
                if (!vSyncEnabled) {
                    for (VkPresentModeKHR mode : modes) {
                        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                            return mode;
                        }
                    }
                    for (VkPresentModeKHR mode : modes) {
                        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                            return mode;
                        }
                    }
                }
                return VK_PRESENT_MODE_FIFO_KHR;
            }

            VkExtent2D VulkanRenderDevice::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
            {
                if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
                    return capabilities.currentExtent;
                }

                int width = 0;
                int height = 0;
                SDL_Vulkan_GetDrawableSize(window, &width, &height);

                VkExtent2D extent{ static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };
                extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
                return extent;
            }

            bool VulkanRenderDevice::CreateSwapchain()
            {
                const SwapChainSupportDetails support = QuerySwapChainSupport(physicalDevice);

                const VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(support.formats);
                const VkPresentModeKHR presentMode = ChooseSwapPresentMode(support.presentModes);
                const VkExtent2D extent = ChooseSwapExtent(support.capabilities);

                swapchainImageFormat = surfaceFormat.format;
                swapchainExtent = extent;

                if (extent.width == 0 || extent.height == 0) {
                    // Window is minimized; defer swapchain creation until it is restored.
                    swapchain = VK_NULL_HANDLE;
                    return true;
                }

                std::uint32_t imageCount = support.capabilities.minImageCount + 1;
                if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
                    imageCount = support.capabilities.maxImageCount;
                }

                VkSwapchainCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                createInfo.surface = surface;
                createInfo.minImageCount = imageCount;
                createInfo.imageFormat = surfaceFormat.format;
                createInfo.imageColorSpace = surfaceFormat.colorSpace;
                createInfo.imageExtent = extent;
                createInfo.imageArrayLayers = 1;
                createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                const std::uint32_t queueFamilyIndices[] = { graphicsQueueFamily, presentQueueFamily };
                if (graphicsQueueFamily != presentQueueFamily) {
                    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                    createInfo.queueFamilyIndexCount = 2;
                    createInfo.pQueueFamilyIndices = queueFamilyIndices;
                }
                else {
                    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                }

                createInfo.preTransform = support.capabilities.currentTransform;
                createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                createInfo.presentMode = presentMode;
                createInfo.clipped = VK_TRUE;
                createInfo.oldSwapchain = VK_NULL_HANDLE;

                if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateSwapchainKHR failed");
                    return false;
                }

                std::uint32_t actualImageCount = 0;
                vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, nullptr);
                swapchainImages.resize(actualImageCount);
                vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data());

                return true;
            }

            bool VulkanRenderDevice::CreateImageViews()
            {
                swapchainImageViews.resize(swapchainImages.size());
                for (std::size_t i = 0; i < swapchainImages.size(); ++i) {
                    VkImageViewCreateInfo createInfo{};
                    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    createInfo.image = swapchainImages[i];
                    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    createInfo.format = swapchainImageFormat;
                    createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
                    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    createInfo.subresourceRange.baseMipLevel = 0;
                    createInfo.subresourceRange.levelCount = 1;
                    createInfo.subresourceRange.baseArrayLayer = 0;
                    createInfo.subresourceRange.layerCount = 1;

                    if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: vkCreateImageView failed");
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateRenderPass()
            {
                VkAttachmentDescription colorAttachment{};
                colorAttachment.format = swapchainImageFormat;
                colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                VkAttachmentReference colorAttachmentRef{};
                colorAttachmentRef.attachment = 0;
                colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorAttachmentRef;

                VkSubpassDependency dependency{};
                dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                dependency.dstSubpass = 0;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask = 0;
                dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                VkRenderPassCreateInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                renderPassInfo.attachmentCount = 1;
                renderPassInfo.pAttachments = &colorAttachment;
                renderPassInfo.subpassCount = 1;
                renderPassInfo.pSubpasses = &subpass;
                renderPassInfo.dependencyCount = 1;
                renderPassInfo.pDependencies = &dependency;

                if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateRenderPass failed");
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateFramebuffers()
            {
                swapchainFramebuffers.resize(swapchainImageViews.size());
                for (std::size_t i = 0; i < swapchainImageViews.size(); ++i) {
                    const VkImageView attachments[] = { swapchainImageViews[i] };

                    VkFramebufferCreateInfo framebufferInfo{};
                    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    framebufferInfo.renderPass = renderPass;
                    framebufferInfo.attachmentCount = 1;
                    framebufferInfo.pAttachments = attachments;
                    framebufferInfo.width = swapchainExtent.width;
                    framebufferInfo.height = swapchainExtent.height;
                    framebufferInfo.layers = 1;

                    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: vkCreateFramebuffer failed");
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateCommandPool()
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = graphicsQueueFamily;

                if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateCommandPool failed");
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateCommandBuffers()
            {
                commandBuffers.resize(kMaxFramesInFlight);

                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = commandPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());

                if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkAllocateCommandBuffers failed");
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateSyncObjects()
            {
                imageAvailableSemaphores.resize(kMaxFramesInFlight);
                inFlightFences.resize(kMaxFramesInFlight);

                VkSemaphoreCreateInfo semaphoreInfo{};
                semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

                for (int i = 0; i < kMaxFramesInFlight; ++i) {
                    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: failed to create synchronization objects for a frame");
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateSwapchainImageSemaphores()
            {
                for (VkSemaphore semaphore : renderFinishedSemaphores) {
                    if (semaphore != VK_NULL_HANDLE) {
                        vkDestroySemaphore(device, semaphore, nullptr);
                    }
                }
                renderFinishedSemaphores.clear();
                imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);

                if (swapchainImages.empty()) {
                    return false;
                }

                renderFinishedSemaphores.resize(swapchainImages.size(), VK_NULL_HANDLE);

                VkSemaphoreCreateInfo semaphoreInfo{};
                semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

                for (std::size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
                    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: failed to create present semaphore for swapchain image");
                        return false;
                    }
                }
                return true;
            }

            void VulkanRenderDevice::CleanupSwapchain()
            {
                for (VkFramebuffer framebuffer : swapchainFramebuffers) {
                    if (framebuffer != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(device, framebuffer, nullptr);
                    }
                }
                swapchainFramebuffers.clear();

                for (VkImageView view : swapchainImageViews) {
                    if (view != VK_NULL_HANDLE) {
                        vkDestroyImageView(device, view, nullptr);
                    }
                }
                swapchainImageViews.clear();

                for (VkSemaphore semaphore : renderFinishedSemaphores) {
                    if (semaphore != VK_NULL_HANDLE) {
                        vkDestroySemaphore(device, semaphore, nullptr);
                    }
                }
                renderFinishedSemaphores.clear();
                imagesInFlight.clear();

                if (swapchain != VK_NULL_HANDLE) {
                    vkDestroySwapchainKHR(device, swapchain, nullptr);
                    swapchain = VK_NULL_HANDLE;
                }

                swapchainImages.clear();
            }

            bool VulkanRenderDevice::RecreateSwapchain()
            {
                int width = 0;
                int height = 0;
                SDL_Vulkan_GetDrawableSize(window, &width, &height);
                if (width == 0 || height == 0) {
                    // Window is minimized; try again on the next Present() call.
                    return false;
                }

                vkDeviceWaitIdle(device);

                CleanupSwapchain();

                if (!CreateSwapchain() || swapchain == VK_NULL_HANDLE) {
                    return false;
                }
                if (!CreateSwapchainImageSemaphores()) {
                    return false;
                }
                if (!CreateImageViews()) {
                    return false;
                }
                if (!CreateFramebuffers()) {
                    return false;
                }

                return true;
            }

        }
    }
}
