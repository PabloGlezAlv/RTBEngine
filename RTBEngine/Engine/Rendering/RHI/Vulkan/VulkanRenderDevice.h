#pragma once

#include "../IRenderDevice.h"
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ImDrawData;

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            // Vulkan backend. Draw* calls made during a frame are recorded into a deferred
            // command list (capturing all bound state + a per-draw uniform snapshot); the
            // list is replayed in Present() as ordered render-pass segments (offscreen
            // targets then swapchain), with optional ImGui draw data at the end of the
            // swapchain pass.
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

                // Shader programs
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

                // Buffers / UBOs
                GpuId CreateBuffer() override;
                void DestroyBuffer(GpuId buffer) override;
                void SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) override;
                void UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size) override;
                void BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint) override;

                // Textures (2D)
                GpuId CreateTexture2D() override;
                void DestroyTexture(GpuId texture) override;
                void SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                     const void* pixels, bool generateMipmaps) override;
                void SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter) override;
                void SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT) override;
                void SetTexture2DDepthShadowParams(GpuId texture) override;
                void BindTexture2D(GpuId texture, unsigned int slot) override;
                void UnbindTexture2D() override;

                // Cubemap
                GpuId CreateCubemap() override;
                void SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                   int width, int height, const void* pixels) override;
                void SetCubemapFilterWrap(GpuId cubemap) override;
                void BindCubemap(GpuId cubemap, unsigned int slot) override;

                // Framebuffers (real offscreen color+depth and depth-only targets)
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

                // Mesh / VAO
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

                bool InitializeImGuiBackend(SDL_Window* window) override;
                void ShutdownImGuiBackend() override;
                void BeginImGuiFrame() override;
                void QueueImGuiDrawData(ImDrawData* drawData) override;
                std::uintptr_t GetNativeTextureIdForImGui(GpuId texture) const override;

                // Accessors for ImGui init / swapchain recreation notifications
                VkInstance GetVkInstance() const { return instance; }
                VkPhysicalDevice GetVkPhysicalDevice() const { return physicalDevice; }
                VkDevice GetVkDevice() const { return device; }
                VkQueue GetGraphicsQueue() const { return graphicsQueue; }
                std::uint32_t GetGraphicsQueueFamily() const { return graphicsQueueFamily; }
                VkRenderPass GetSwapchainRenderPass() const { return renderPass; }
                std::uint32_t GetSwapchainImageCount() const { return static_cast<std::uint32_t>(swapchainImages.size()); }
                std::uint32_t GetMinImageCount() const { return 2; }

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

                struct PerDrawCPU {
                    alignas(16) float uModel[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    alignas(16) float uLightSpaceMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    alignas(16) float uViewProjection[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    alignas(16) float uColor[4] = { 1,1,1,1 };
                    alignas(16) float uDiffuseColor[3] = { 1,1,1 };
                    float uShininess = 32.0f;
                    alignas(16) float uWorldPosition[3] = { 0,0,0 };
                    float uVerticalOffset = 0.0f;
                    alignas(8) float uSize[2] = { 1,1 };
                    float uFrame = 0.0f;
                    float uShadowBias = 0.005f;
                    std::int32_t uHasTexture = 0;
                    std::int32_t uUseInstancing = 0;
                    std::int32_t uHasAnimation = 0;
                    std::int32_t uUseInstanceColor = 0;
                    std::int32_t uHasShadows = 0;
                    std::int32_t uSheetEnabled = 0;
                    std::int32_t uSheetColumns = 1;
                    std::int32_t uSheetRows = 1;
                    std::int32_t uSheetFrameCount = 1;
                    float uTime = 0.0f;
                    float uPulseSpeed = 1.0f;
                    float uGlowIntensity = 1.0f;
                    float _padEnd = 0.0f;
                };
                static_assert(sizeof(PerDrawCPU) % 16 == 0, "PerDrawCPU must be a multiple of 16");
                static_assert(alignof(PerDrawCPU) >= 16, "PerDrawCPU must be 16-byte aligned");

                struct PerDrawField {
                    const char* name;
                    const char* glslType;
                    int offset;
                    bool isBool;
                };

                struct VertexAttribBinding {
                    GpuId buffer = kInvalidGpuId;
                    int components = 0;
                    int stride = 0;
                    std::size_t offset = 0;
                    bool isInt = false;
                    unsigned int divisor = 0;
                };

                struct VaoResource {
                    std::unordered_map<unsigned int, VertexAttribBinding> attributes;
                    GpuId elementBuffer = kInvalidGpuId;
                    std::uint32_t generation = 0;
                };

                struct BufferResource {
                    VkBuffer buffer = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                    VkDeviceSize size = 0;
                    VkBufferUsageFlags usage = 0;
                };

                struct TextureResource {
                    VkImage image = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                    VkImageView view = VK_NULL_HANDLE;
                    VkSampler sampler = VK_NULL_HANDLE;
                    int width = 0;
                    int height = 0;
                    VkFormat format = VK_FORMAT_UNDEFINED;
                    bool isCubemap = false;
                    std::uint32_t layerCount = 1;
                    bool isDepth = false;
                    TextureFilter minFilter = TextureFilter::Linear;
                    TextureFilter magFilter = TextureFilter::Linear;
                    TextureWrap wrapS = TextureWrap::Repeat;
                    TextureWrap wrapT = TextureWrap::Repeat;
                    bool depthCompareDisabled = true;
                };

                struct ShaderProgramResource {
                    VkShaderModule vertModule = VK_NULL_HANDLE;
                    VkShaderModule fragModule = VK_NULL_HANDLE;
                    bool valid = false;
                    // Locations declared as `layout(location=N) in` in the vertex shader.
                    // Used to omit unused VAO attrs (avoids Vulkan validation spam).
                    std::vector<unsigned int> usedVertexLocations;
                };

                struct FramebufferResource {
                    GpuId colorTexture = kInvalidGpuId;
                    GpuId depthTexture = kInvalidGpuId;
                    VkRenderPass renderPass = VK_NULL_HANDLE;
                    VkFramebuffer framebuffer = VK_NULL_HANDLE;
                    int width = 0;
                    int height = 0;
                    bool depthOnly = false;
                    bool complete = false;
                    ClearMask pendingClearMask = ClearMask::None;
                    float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
                };

                struct PipelineKey {
                    GpuId program = kInvalidGpuId;
                    GpuId vao = kInvalidGpuId;
                    std::uint32_t vaoGeneration = 0;
                    int topology = 0;
                    bool depthTest = true;
                    bool depthWrite = true;
                    int depthFunc = 0;
                    bool cullFace = true;
                    bool blend = false;
                    int srcRGB = 1, dstRGB = 0, srcAlpha = 1, dstAlpha = 0;
                    bool colorMask[4] = { true, true, true, true };
                    GpuId targetFramebuffer = 0; // 0 = swapchain
                    bool depthOnly = false;

                    bool operator==(const PipelineKey& other) const;
                };

                struct PipelineKeyHash {
                    std::size_t operator()(const PipelineKey& key) const;
                };

                struct DrawCommand {
                    GpuId program = kInvalidGpuId;
                    GpuId vao = kInvalidGpuId;
                    std::uint32_t vaoGeneration = 0;
                    PrimitiveTopology topology = PrimitiveTopology::Triangles;
                    bool indexed = true;
                    IndexType indexType = IndexType::UInt32;
                    int count = 0;
                    int first = 0;
                    int instanceCount = 1;

                    bool depthTest = true;
                    bool depthWrite = true;
                    DepthFunc depthFunc = DepthFunc::Less;
                    bool cullFace = true;
                    bool blend = false;
                    int srcRGB = 1, dstRGB = 0, srcAlpha = 1, dstAlpha = 0;
                    bool colorMask[4] = { true, true, true, true };
                    int viewport[4] = { 0, 0, 0, 0 };

                    GpuId uboLighting = kInvalidGpuId;
                    GpuId uboCamera = kInvalidGpuId;
                    GpuId uboBone = kInvalidGpuId;
                    // Resolved at record time so later Upload() cannot overwrite data
                    // still referenced by deferred draws awaiting Present().
                    VkBuffer vkLighting = VK_NULL_HANDLE;
                    VkBuffer vkCamera = VK_NULL_HANDLE;
                    VkBuffer vkBone = VK_NULL_HANDLE;
                    // Vertex/index VkBuffers snapshotted like UBOs — shared dynamic VBOs
                    // (world UI) are orphaned+rewritten between DrawArrays in one frame.
                    std::vector<VkBuffer> vkVertexBuffers;
                    VkBuffer vkIndexBuffer = VK_NULL_HANDLE;
                    GpuId texSlot0 = kInvalidGpuId;
                    GpuId texSlot1 = kInvalidGpuId;
                    GpuId cubemapSlot0 = kInvalidGpuId;

                    GpuId targetFramebuffer = 0; // 0 = swapchain
                    bool depthOnly = false;

                    PerDrawCPU perDraw;
                };

                struct OrphanedBuffer {
                    VkBuffer buffer = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                };

                struct OrphanedTexture {
                    VkImage image = VK_NULL_HANDLE;
                    VkDeviceMemory memory = VK_NULL_HANDLE;
                    VkImageView view = VK_NULL_HANDLE;
                    VkSampler sampler = VK_NULL_HANDLE;
                };

                bool CreateInstance();
                bool SetupDebugMessenger();
                bool CreateSurface();
                bool PickPhysicalDevice();
                bool CreateLogicalDevice();
                bool CreateSwapchain();
                bool CreateImageViews();
                bool CreateRenderPass();
                bool CreateDepthResources();
                void CleanupDepthResources();
                bool CreateFramebuffers();
                bool CreateCommandPool();
                bool CreateCommandBuffers();
                bool CreateSyncObjects();
                bool CreateSwapchainImageSemaphores();
                bool RecreateSwapchain();
                void CleanupSwapchain();

                bool CreateDescriptorSetLayoutAndPipelineLayout();
                bool CreatePerDrawBuffers();
                void CleanupPerDrawBuffers();
                bool CreateFallbackResources();
                void CleanupFallbackResources();
                void DestroyAllPipelines();

                void DestroyFramebufferGpu(GpuId framebufferId, FramebufferResource& fb);
                bool RebuildFramebufferGpu(GpuId framebufferId, FramebufferResource& fb);
                bool CreateOffscreenColorDepthRenderPass(VkFormat colorFmt, VkFormat depthFmt, VkRenderPass& outPass) const;
                bool CreateOffscreenDepthOnlyRenderPass(VkFormat depthFmt, VkRenderPass& outPass) const;
                void InvalidatePipelinesForFramebuffer(GpuId framebufferId);
                static std::vector<unsigned int> ParseVertexInputLocations(const std::string& vertexSource);

                void ReplayDraw(VkCommandBuffer cmd, const DrawCommand& draw, std::uint32_t drawSlot);
                bool BeginTargetRenderPass(VkCommandBuffer cmd, GpuId target, std::uint32_t swapImageIndex,
                                          float clearCol[4], bool& inPass, GpuId& activeTarget);
                void EndActiveRenderPass(VkCommandBuffer cmd, bool& inPass, GpuId endingTarget = kInvalidGpuId);

                std::vector<const char*> GetRequiredInstanceExtensions();
                bool CheckValidationLayerSupport() const;
                QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
                SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice candidate) const;
                bool IsDeviceSuitable(VkPhysicalDevice candidate) const;
                VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
                VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
                VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
                VkFormat FindSupportedDepthFormat() const;

                std::uint32_t FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags props) const;
                bool CreateBufferRaw(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                    VkBuffer& outBuffer, VkDeviceMemory& outMemory) const;
                void EnsureBufferCapacity(GpuId id, std::size_t size, VkBufferUsageFlags usage);
                void UploadHostVisibleBuffer(VkDeviceMemory memory, const void* data, std::size_t size) const;
                VkCommandBuffer BeginSingleTimeCommands() const;
                void EndSingleTimeCommands(VkCommandBuffer cmd) const;
                bool CreateImageRaw(std::uint32_t width, std::uint32_t height, VkFormat format, VkImageTiling tiling,
                                   VkImageUsageFlags usage, VkMemoryPropertyFlags props, std::uint32_t arrayLayers,
                                   VkImageCreateFlags flags, VkImage& outImage, VkDeviceMemory& outMemory) const;
                VkImageView CreateImageViewRaw(VkImage image, VkFormat format, VkImageAspectFlags aspect,
                                               VkImageViewType viewType, std::uint32_t layerCount) const;
                void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                          VkImageAspectFlags aspect, std::uint32_t layerCount) const;
                void CopyBufferToImage(VkBuffer buffer, VkImage image, std::uint32_t width, std::uint32_t height,
                                      std::uint32_t baseLayer) const;
                VkSampler CreateSamplerRaw(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode addressMode,
                                          bool depthCompare) const;
                void RecreateSampler(TextureResource& res);
                static VkFormat ToVkFormat(TextureFormat format, bool& isDepth);
                static VkFilter ToVkFilter(TextureFilter filter);
                static VkSamplerAddressMode ToVkWrap(TextureWrap wrap);
                static VkPrimitiveTopology ToVkTopology(PrimitiveTopology topology);
                static VkCompareOp ToVkCompareOp(DepthFunc func);
                static VkBlendFactor ToVkBlendFactor(int glBlendFactor);

                VkPipeline GetOrCreatePipeline(const DrawCommand& cmd);
                VkRenderPass ResolveRenderPassForTarget(GpuId targetFramebuffer) const;
                static std::vector<GpuId> ComputeVaoBufferOrder(const VaoResource& vao);
                std::string TransformShaderSource(const std::string& source, bool isFragment) const;
                static const PerDrawField* FindPerDrawField(const char* name);
                void* PerDrawFieldPtr(int location);
                void RecordDrawCommand(PrimitiveTopology topology, bool indexed, IndexType indexType,
                                      int count, int first, int instanceCount);
                void RemoveImGuiTexture(GpuId texture);
                void OrphanUniformBufferIfDeferred(GpuId buffer);
                void RetireOrphanedBuffers();
                void OrphanTextureGpuResources(TextureResource& res);
                void ClampPendingDrawViewportsToFramebuffer(GpuId framebufferId, const FramebufferResource& fb);
                void NoteBufferInFlight(VkBuffer buffer);
                bool IsBufferInFlight(VkBuffer buffer) const;
                bool IsBufferReferencedByPendingDraws(VkBuffer buffer) const;
                bool IsGpuBufferUsedByPendingVaos(GpuId buffer) const;
                void OrphanBufferIfInFlight(GpuId buffer);
                void FlushDeferredResourceDestroys();
                VkBuffer ResolveBufferHandle(GpuId id) const;

                static constexpr int kMaxFramesInFlight = 2;
                static constexpr std::uint32_t kMaxDrawsPerFrame = 8192;

                static constexpr std::uint32_t kDescLighting = 0;
                static constexpr std::uint32_t kDescCamera = 1;
                static constexpr std::uint32_t kDescBone = 2;
                static constexpr std::uint32_t kDescPerDraw = 3;
                static constexpr std::uint32_t kDescTexture0 = 4;
                static constexpr std::uint32_t kDescTexture1 = 5;
                static constexpr std::uint32_t kDescCubemap = 6;
                static constexpr std::uint32_t kDescriptorBindingCount = 7;

                SDL_Window* window = nullptr;
                bool vSyncEnabled = true;
                bool initialized = false;
                bool debugUtilsAvailable = false;
                std::uint32_t vulkanApiVersion = VK_API_VERSION_1_1;

                VkInstance instance = VK_NULL_HANDLE;
                VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
                VkSurfaceKHR surface = VK_NULL_HANDLE;
                VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
                VkDevice device = VK_NULL_HANDLE;
                VkQueue graphicsQueue = VK_NULL_HANDLE;
                VkQueue presentQueue = VK_NULL_HANDLE;
                std::uint32_t graphicsQueueFamily = 0;
                std::uint32_t presentQueueFamily = 0;
                VkDeviceSize uniformBufferAlignment = 256;

                VkSwapchainKHR swapchain = VK_NULL_HANDLE;
                std::vector<VkImage> swapchainImages;
                std::vector<VkImageView> swapchainImageViews;
                VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
                VkExtent2D swapchainExtent{ 0, 0 };

                VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
                std::vector<VkImage> depthImages;
                std::vector<VkDeviceMemory> depthImageMemories;
                std::vector<VkImageView> depthImageViews;

                VkRenderPass renderPass = VK_NULL_HANDLE;
                std::vector<VkFramebuffer> swapchainFramebuffers;

                VkCommandPool commandPool = VK_NULL_HANDLE;
                std::vector<VkCommandBuffer> commandBuffers;

                std::vector<VkSemaphore> imageAvailableSemaphores;
                std::vector<VkSemaphore> renderFinishedSemaphores;
                std::vector<VkFence> inFlightFences;
                std::vector<VkFence> imagesInFlight;
                std::size_t currentFrame = 0;

                VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
                VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
                std::array<VkDescriptorPool, kMaxFramesInFlight> descriptorPools{};
                std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> pipelineCache;

                std::array<VkBuffer, kMaxFramesInFlight> perDrawBuffers{};
                std::array<VkDeviceMemory, kMaxFramesInFlight> perDrawMemories{};
                std::array<void*, kMaxFramesInFlight> perDrawMapped{};
                VkDeviceSize perDrawSlotStride = 256;

                VkBuffer fallbackUBOBuffer = VK_NULL_HANDLE;
                VkDeviceMemory fallbackUBOMemory = VK_NULL_HANDLE;
                VkDeviceSize fallbackUBOSize = 16384;
                VkBuffer fallbackVertexBuffer = VK_NULL_HANDLE;
                VkDeviceMemory fallbackVertexMemory = VK_NULL_HANDLE;
                TextureResource fallbackTexture2D;
                TextureResource fallbackCubemap;

                void* shadercCompiler = nullptr;

                float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
                ClearMask pendingClearMask = ClearMask::None;
                int pendingViewport[4] = { 0, 0, 0, 0 };

                GpuId currentProgram = kInvalidGpuId;
                GpuId currentVAO = kInvalidGpuId;
                GpuId currentArrayBuffer = kInvalidGpuId;
                GpuId currentElementBuffer = kInvalidGpuId;
                GpuId currentBoundFramebuffer = 0; // 0 = swapchain

                bool stateDepthTest = true;
                bool stateDepthWrite = true;
                DepthFunc stateDepthFunc = DepthFunc::Less;
                bool stateCullFace = true;
                bool stateBlend = false;
                int stateSrcRGB = 1, stateDstRGB = 0, stateSrcAlpha = 1, stateDstAlpha = 0;
                bool stateColorMask[4] = { true, true, true, true };

                GpuId boundUBO[3] = { kInvalidGpuId, kInvalidGpuId, kInvalidGpuId };
                std::unordered_map<unsigned int, GpuId> boundTextureSlots;
                GpuId boundCubemapSlot0 = kInvalidGpuId;

                PerDrawCPU currentPerDraw;

                std::vector<DrawCommand> pendingDraws;
                // Buffers orphaned while deferred draws still hold their VkBuffer handles.
                // pendingOrphans → moved into orphanedBuffersByFrame[currentFrame] at Present,
                // retired after that frame's fence is waited on the next time around.
                std::vector<OrphanedBuffer> pendingOrphans;
                std::array<std::vector<OrphanedBuffer>, kMaxFramesInFlight> orphanedBuffersByFrame{};
                std::vector<OrphanedTexture> pendingTextureOrphans;
                std::array<std::vector<OrphanedTexture>, kMaxFramesInFlight> orphanedTexturesByFrame{};
                // Host-visible VkBuffers referenced by submitted command buffers. Writing them
                // before the fence signals causes VK_ERROR_DEVICE_LOST (-4).
                std::array<std::vector<VkBuffer>, kMaxFramesInFlight> buffersInFlightByFrame{};
                // GpuIds whose CPU-side maps must stay alive until Present replays pending draws.
                std::vector<GpuId> deferredBufferDestroys;
                std::vector<GpuId> deferredFramebufferDestroys;
                std::vector<GpuId> deferredVaoDestroys;
                ImDrawData* pendingImGuiDrawData = nullptr;
                bool imguiBackendInitialized = false;
                mutable std::unordered_map<GpuId, VkDescriptorSet> imguiTextureSets;

                std::unordered_map<GpuId, BufferResource> buffers;
                std::unordered_map<GpuId, TextureResource> textures;
                std::unordered_map<GpuId, VaoResource> vaos;
                std::unordered_map<GpuId, ShaderProgramResource> programs;
                std::unordered_map<GpuId, FramebufferResource> framebuffers;
                GpuId nextId = 1;
            };

        }
    }
}
