#pragma once

#include "../IRenderDevice.h"
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            // Minimal Vulkan backend (MVP): initializes an instance/device/swapchain and can
            // clear + present a framebuffer. Resource creation (shaders, buffers, textures, ...)
            // is stubbed out with fake ids so higher-level engine code (ResourceManager, etc.)
            // can run without a full Vulkan pipeline implementation yet.
            class VulkanRenderDevice final : public IRenderDevice {
            public:
                VulkanRenderDevice() = default;
                ~VulkanRenderDevice() override;

                bool Initialize(SDL_Window* window, bool vSync) override;
                void Shutdown() override;
                GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }

                void MakeCurrent() override;
                void Present() override;
                void SetVSync(bool enabled) override;
                void* GetNativeContext() const override { return nullptr; }

                // Frame / raster state
                void SetViewport(int x, int y, int width, int height) override;
                void SetClearColor(float r, float g, float b, float a = 1.0f) override;
                void Clear(ClearMask mask) override;
                void SetDepthTest(bool enabled) override;
                void SetDepthFunc(DepthFunc func) override;
                void SetDepthWrite(bool enabled) override;
                void SetCullFace(bool enabled) override;
                void SetBlend(bool enabled) override;
                void SetBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha) override;
                void SetColorMask(bool red, bool green, bool blue, bool alpha) override;

                // Shader programs (stubbed)
                GpuId CreateShaderProgram(const std::string& vertexSource,
                                          const std::string& fragmentSource) override;
                void DestroyShaderProgram(GpuId program) override;
                void BindShaderProgram(GpuId program) override;
                int GetUniformLocation(GpuId program, const char* name) override;
                void SetUniformBool(int location, bool value) override;
                void SetUniformInt(int location, int value) override;
                void SetUniformFloat(int location, float value) override;
                void SetUniformVec2(int location, float x, float y) override;
                void SetUniformVec3(int location, float x, float y, float z) override;
                void SetUniformVec4(int location, float x, float y, float z, float w) override;
                void SetUniformMat4(int location, const float* matrix4x4) override;
                void BindUniformBlock(GpuId program, const char* blockName, unsigned int bindingPoint) override;

                // Buffers / UBOs (stubbed)
                GpuId CreateBuffer() override;
                void DestroyBuffer(GpuId buffer) override;
                void SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) override;
                void UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size) override;
                void BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint) override;

                // Textures (2D) (stubbed)
                GpuId CreateTexture2D() override;
                void DestroyTexture(GpuId texture) override;
                void SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                     const void* pixels, bool generateMipmaps) override;
                void SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter) override;
                void SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT) override;
                void SetTexture2DDepthShadowParams(GpuId texture) override;
                void BindTexture2D(GpuId texture, unsigned int slot) override;
                void UnbindTexture2D() override;

                // Cubemap (stubbed)
                GpuId CreateCubemap() override;
                void SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                   int width, int height, const void* pixels) override;
                void SetCubemapFilterWrap(GpuId cubemap) override;
                void BindCubemap(GpuId cubemap, unsigned int slot) override;

                // Framebuffers (stubbed)
                GpuId CreateFramebuffer() override;
                void DestroyFramebuffer(GpuId framebuffer) override;
                void BindFramebuffer(GpuId framebuffer) override;
                void UnbindFramebuffer() override;
                void AttachFramebufferColorTexture(GpuId framebuffer, GpuId texture) override;
                void AttachFramebufferDepthTexture(GpuId framebuffer, GpuId texture) override;
                void SetFramebufferDrawReadNone() override;
                bool IsFramebufferComplete() const override;
                GpuId CreateColorTextureForFramebuffer(int width, int height) override;
                GpuId CreateDepthTextureForFramebuffer(int width, int height) override;

                // Mesh / VAO (stubbed)
                GpuId CreateVertexArray() override;
                void DestroyVertexArray(GpuId vao) override;
                void BindVertexArray(GpuId vao) override;
                void UnbindVertexArray() override;
                void SetArrayBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) override;
                void SetElementBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) override;
                void BindArrayBuffer(GpuId buffer) override;
                void BindElementBuffer(GpuId buffer) override;
                void EnableVertexAttribFloat(unsigned int location, int components, int stride, std::size_t offset) override;
                void EnableVertexAttribInt(unsigned int location, int components, int stride, std::size_t offset) override;
                void SetVertexAttribDivisor(unsigned int location, unsigned int divisor) override;
                void DrawIndexed(PrimitiveTopology topology, int indexCount, IndexType indexType) override;
                void DrawIndexedInstanced(PrimitiveTopology topology, int indexCount, IndexType indexType, int instanceCount) override;
                void DrawArrays(PrimitiveTopology topology, int first, int count) override;
                void DrawArraysInstanced(PrimitiveTopology topology, int first, int count, int instanceCount) override;

                unsigned int GetNativeTextureIdForImGui(GpuId texture) const override;

            private:
                struct QueueFamilyIndices {
                    bool hasGraphics = false;
                    bool hasPresent = false;
                    std::uint32_t graphicsFamily = 0;
                    std::uint32_t presentFamily = 0;
                    bool IsComplete() const { return hasGraphics && hasPresent; }
                };

                struct SwapChainSupportDetails {
                    VkSurfaceCapabilitiesKHR capabilities{};
                    std::vector<VkSurfaceFormatKHR> formats;
                    std::vector<VkPresentModeKHR> presentModes;
                };

                bool CreateInstance();
                bool SetupDebugMessenger();
                bool CreateSurface();
                bool PickPhysicalDevice();
                bool CreateLogicalDevice();
                bool CreateSwapchain();
                bool CreateImageViews();
                bool CreateRenderPass();
                bool CreateFramebuffers();
                bool CreateCommandPool();
                bool CreateCommandBuffers();
                bool CreateSyncObjects();
                bool CreateSwapchainImageSemaphores();
                bool RecreateSwapchain();
                void CleanupSwapchain();

                std::vector<const char*> GetRequiredInstanceExtensions();
                bool CheckValidationLayerSupport() const;
                QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
                SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice candidate) const;
                bool IsDeviceSuitable(VkPhysicalDevice candidate) const;
                VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
                VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
                VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

                static constexpr int kMaxFramesInFlight = 2;

                SDL_Window* window = nullptr;
                bool vSyncEnabled = true;
                bool initialized = false;
                bool debugUtilsAvailable = false;

                VkInstance instance = VK_NULL_HANDLE;
                VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
                VkSurfaceKHR surface = VK_NULL_HANDLE;
                VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
                VkDevice device = VK_NULL_HANDLE;
                VkQueue graphicsQueue = VK_NULL_HANDLE;
                VkQueue presentQueue = VK_NULL_HANDLE;
                std::uint32_t graphicsQueueFamily = 0;
                std::uint32_t presentQueueFamily = 0;

                VkSwapchainKHR swapchain = VK_NULL_HANDLE;
                std::vector<VkImage> swapchainImages;
                std::vector<VkImageView> swapchainImageViews;
                VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
                VkExtent2D swapchainExtent{ 0, 0 };

                VkRenderPass renderPass = VK_NULL_HANDLE;
                std::vector<VkFramebuffer> swapchainFramebuffers;

                VkCommandPool commandPool = VK_NULL_HANDLE;
                std::vector<VkCommandBuffer> commandBuffers;

                std::vector<VkSemaphore> imageAvailableSemaphores;   // per frame-in-flight
                std::vector<VkSemaphore> renderFinishedSemaphores;   // per swapchain image
                std::vector<VkFence> inFlightFences;                 // per frame-in-flight
                std::vector<VkFence> imagesInFlight;                 // per swapchain image (non-owning)
                std::size_t currentFrame = 0;

                float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
                ClearMask pendingClearMask = ClearMask::None;
                int pendingViewport[4] = { 0, 0, 0, 0 };

                // Fake GPU resource ids handed out to unblock resource-loading code paths
                // that don't yet have a real Vulkan-backed implementation.
                std::uint32_t nextId = 1;
            };

        }
    }
}
