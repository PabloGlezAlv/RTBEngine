#include "VulkanRenderDevice.h"
#include "VulkanGiContext.h"
#include "../RenderDevice.h"
#include "../../../Core/Logger.h"
#include <SDL_vulkan.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <shaderc/shaderc.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

                // GLSL bool in std140 is 4 bytes (same as int / our PerDrawCPU int32 fields).
                constexpr const char* kPerDrawBlockGlsl =
                    "layout(std140, set = 0, binding = 3) uniform PerDraw {\n"
                    "    mat4 uModel;\n"
                    "    mat4 uLightSpaceMatrix;\n"
                    "    mat4 uViewProjection;\n"
                    "    vec4 uColor;\n"
                    "    vec3 uDiffuseColor;\n"
                    "    float uShininess;\n"
                    "    vec3 uWorldPosition;\n"
                    "    float uVerticalOffset;\n"
                    "    vec2 uSize;\n"
                    "    float uFrame;\n"
                    "    float uShadowBias;\n"
                    "    bool uHasTexture;\n"
                    "    bool uUseInstancing;\n"
                    "    bool uHasAnimation;\n"
                    "    bool uUseInstanceColor;\n"
                    "    bool uHasShadows;\n"
                    "    bool uSheetEnabled;\n"
                    "    int uSheetColumns;\n"
                    "    int uSheetRows;\n"
                    "    int uSheetFrameCount;\n"
                    "    float uTime;\n"
                    "    float uPulseSpeed;\n"
                    "    float uGlowIntensity;\n"
                    "    bool uFogEnabled;\n"
                    "    float uFogDensity;\n"
                    "    float uFogHeight;\n"
                    "    float uFogHeightFalloff;\n"
                    "    vec3 uFogColor;\n"
                    "    float uFogStart;\n"
                    "    float uFogEnd;\n"
                    "    bool uVolumetricFogEnabled;\n"
                    "    float uVolumetricIntensity;\n"
                    "    float uVolumetricAnisotropy;\n"
                    "    int uVolumetricSamples;\n"
                    "    float uCameraNear;\n"
                    "    float uCameraFar;\n"
                    "    bool uDepthZeroToOne;\n"
                    "    float uVolumetricMaxLuminance;\n"
                    "};\n";

                constexpr const char* kSamplerBindingsGlsl =
                    "layout(set = 0, binding = 4) uniform sampler2D uTexture;\n"
                    "layout(set = 0, binding = 5) uniform sampler2D uShadowMap;\n"
                    "layout(set = 0, binding = 6) uniform samplerCube uSkybox;\n"
                    "layout(set = 0, binding = 8) uniform sampler2D uDDGIIrradiance;\n"
                    "layout(set = 0, binding = 9) uniform sampler2D uDDGIDistance;\n"
                    "#define uDiffuse uTexture\n"
                    "#define uSceneDepth uTexture\n";

                const char* kLooseUniformNames[] = {
                    "uModel", "uLightSpaceMatrix", "uViewProjection", "uColor", "uDiffuseColor", "uShininess",
                    "uWorldPosition", "uVerticalOffset", "uSize", "uFrame", "uShadowBias",
                    "uHasTexture", "uUseInstancing", "uHasAnimation", "uUseInstanceColor",
                    "uHasShadows", "uSheetEnabled", "uSheetColumns", "uSheetRows", "uSheetFrameCount",
                    "uTime", "uPulseSpeed", "uGlowIntensity",
                    "uFogEnabled", "uFogDensity", "uFogHeight", "uFogHeightFalloff", "uFogColor",
                    "uFogStart", "uFogEnd", "uVolumetricFogEnabled", "uVolumetricIntensity",
                    "uVolumetricAnisotropy", "uVolumetricSamples", "uCameraNear", "uCameraFar",
                    "uDepthZeroToOne", "uVolumetricMaxLuminance",
                    "uTexture", "uShadowMap", "uSkybox", "uDiffuse", "uSceneDepth",
                    "uDDGIIrradiance", "uDDGIDistance", "uDDGIEnabled"
                };

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
                    return func ? func(vkInstance, createInfo, allocator, messenger)
                                : VK_ERROR_EXTENSION_NOT_PRESENT;
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

            } // anonymous

            // ---------------------------------------------------------------------------
            // PipelineKey
            // ---------------------------------------------------------------------------

            bool VulkanRenderDevice::PipelineKey::operator==(const PipelineKey& other) const
            {
                return program == other.program
                    && vao == other.vao
                    && vaoGeneration == other.vaoGeneration
                    && topology == other.topology
                    && depthTest == other.depthTest
                    && depthWrite == other.depthWrite
                    && depthFunc == other.depthFunc
                    && cullFace == other.cullFace
                    && blend == other.blend
                    && srcRGB == other.srcRGB && dstRGB == other.dstRGB
                    && srcAlpha == other.srcAlpha && dstAlpha == other.dstAlpha
                    && colorMask[0] == other.colorMask[0] && colorMask[1] == other.colorMask[1]
                    && colorMask[2] == other.colorMask[2] && colorMask[3] == other.colorMask[3]
                    && targetFramebuffer == other.targetFramebuffer
                    && depthOnly == other.depthOnly;
            }

            std::size_t VulkanRenderDevice::PipelineKeyHash::operator()(const PipelineKey& key) const
            {
                std::size_t h = 0;
                auto mix = [&](std::size_t v) {
                    h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
                };
                mix(static_cast<std::size_t>(key.program));
                mix(static_cast<std::size_t>(key.vao));
                mix(key.vaoGeneration);
                mix(static_cast<std::size_t>(key.topology));
                mix(key.depthTest ? 1u : 0u);
                mix(key.depthWrite ? 1u : 0u);
                mix(static_cast<std::size_t>(key.depthFunc));
                mix(key.cullFace ? 1u : 0u);
                mix(key.blend ? 1u : 0u);
                mix(static_cast<std::size_t>(key.srcRGB));
                mix(static_cast<std::size_t>(key.dstRGB));
                mix(static_cast<std::size_t>(key.targetFramebuffer));
                mix(key.depthOnly ? 1u : 0u);
                return h;
            }

            // ---------------------------------------------------------------------------
            // Lifecycle
            // ---------------------------------------------------------------------------

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
                pendingDraws.reserve(1024);

                shadercCompiler = shaderc_compiler_initialize();
                if (!shadercCompiler) {
                    RTB_ERROR("VulkanRenderDevice: shaderc_compiler_initialize failed");
                    return false;
                }

                if (!CreateInstance() || !SetupDebugMessenger() || !CreateSurface()
                    || !PickPhysicalDevice() || !CreateLogicalDevice()) {
                    Shutdown();
                    return false;
                }

                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(physicalDevice, &props);
                uniformBufferAlignment = props.limits.minUniformBufferOffsetAlignment;
                if (uniformBufferAlignment == 0) {
                    uniformBufferAlignment = 256;
                }
                perDrawSlotStride = ((sizeof(PerDrawCPU) + uniformBufferAlignment - 1) / uniformBufferAlignment)
                    * uniformBufferAlignment;

                if (!CreateSwapchain() || swapchain == VK_NULL_HANDLE) {
                    RTB_ERROR("VulkanRenderDevice: failed to create swapchain");
                    Shutdown();
                    return false;
                }
                // Command pool must exist before depth/fallback image transitions.
                if (!CreateCommandPool() || !CreateCommandBuffers() || !CreateSyncObjects()
                    || !CreateSwapchainImageSemaphores() || !CreateImageViews()
                    || !CreateRenderPass() || !CreateDepthResources() || !CreateFramebuffers()
                    || !CreateDescriptorSetLayoutAndPipelineLayout() || !CreatePerDrawBuffers()
                    || !CreateFallbackResources()) {
                    Shutdown();
                    return false;
                }

                pendingViewport[0] = 0;
                pendingViewport[1] = 0;
                pendingViewport[2] = static_cast<int>(swapchainExtent.width);
                pendingViewport[3] = static_cast<int>(swapchainExtent.height);

                initialized = true;
                giContext = std::make_unique<VulkanGiContext>(*this);
                giContext->Initialize(physicalDevice, device, graphicsQueueFamily, graphicsQueue);
                RTB_INFO("VulkanRenderDevice initialized (depth + deferred draws + SPIR-V)");
                return true;
            }

            void VulkanRenderDevice::Shutdown()
            {

                if (device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(device);
                }

                if (giContext) {
                    giContext->Shutdown();
                    giContext.reset();
                }

                pendingDraws.clear();
                pendingImGuiDrawData = nullptr;
                // ImGui backend must already be shut down by Application before device destruction.
                if (imguiBackendInitialized) {
                    ShutdownImGuiBackend();
                }
                for (auto& [id, set] : imguiTextureSets) {
                    (void)id;
                    (void)set;
                }
                imguiTextureSets.clear();
                DestroyAllPipelines();

                for (auto& [id, fb] : framebuffers) {
                    DestroyFramebufferGpu(id, fb);
                }
                framebuffers.clear();

                for (auto& [id, prog] : programs) {
                    if (prog.vertModule) vkDestroyShaderModule(device, prog.vertModule, nullptr);
                    if (prog.fragModule) vkDestroyShaderModule(device, prog.fragModule, nullptr);
                }
                programs.clear();

                for (auto& [id, tex] : textures) {
                    if (tex.sampler) vkDestroySampler(device, tex.sampler, nullptr);
                    if (tex.view) vkDestroyImageView(device, tex.view, nullptr);
                    if (tex.image) vkDestroyImage(device, tex.image, nullptr);
                    if (tex.memory) vkFreeMemory(device, tex.memory, nullptr);
                }
                textures.clear();

                auto destroyOrphans = [&](std::vector<OrphanedBuffer>& orphans) {
                    for (OrphanedBuffer& orphan : orphans) {
                        if (orphan.buffer) vkDestroyBuffer(device, orphan.buffer, nullptr);
                        if (orphan.memory) vkFreeMemory(device, orphan.memory, nullptr);
                    }
                    orphans.clear();
                };
                destroyOrphans(pendingOrphans);
                for (auto& frameOrphans : orphanedBuffersByFrame) {
                    destroyOrphans(frameOrphans);
                }

                auto destroyTexOrphans = [&](std::vector<OrphanedTexture>& orphans) {
                    for (OrphanedTexture& orphan : orphans) {
                        if (orphan.sampler) vkDestroySampler(device, orphan.sampler, nullptr);
                        if (orphan.view) vkDestroyImageView(device, orphan.view, nullptr);
                        if (orphan.image) vkDestroyImage(device, orphan.image, nullptr);
                        if (orphan.memory) vkFreeMemory(device, orphan.memory, nullptr);
                    }
                    orphans.clear();
                };
                destroyTexOrphans(pendingTextureOrphans);
                for (auto& frameOrphans : orphanedTexturesByFrame) {
                    destroyTexOrphans(frameOrphans);
                }

                for (auto& [id, buf] : buffers) {
                    if (buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
                    if (buf.memory) vkFreeMemory(device, buf.memory, nullptr);
                }
                buffers.clear();
                vaos.clear();

                CleanupFallbackResources();
                CleanupPerDrawBuffers();

                if (pipelineLayout) {
                    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
                    pipelineLayout = VK_NULL_HANDLE;
                }
                if (descriptorSetLayout) {
                    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                    descriptorSetLayout = VK_NULL_HANDLE;
                }
                for (auto& pool : descriptorPools) {
                    if (pool) {
                        vkDestroyDescriptorPool(device, pool, nullptr);
                        pool = VK_NULL_HANDLE;
                    }
                }

                for (VkFence fence : inFlightFences) {
                    if (fence) vkDestroyFence(device, fence, nullptr);
                }
                inFlightFences.clear();
                imagesInFlight.clear();

                for (VkSemaphore semaphore : imageAvailableSemaphores) {
                    if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
                }
                imageAvailableSemaphores.clear();

                if (commandPool) {
                    vkDestroyCommandPool(device, commandPool, nullptr);
                    commandPool = VK_NULL_HANDLE;
                }
                commandBuffers.clear();

                CleanupSwapchain();
                CleanupDepthResources();

                if (renderPass) {
                    vkDestroyRenderPass(device, renderPass, nullptr);
                    renderPass = VK_NULL_HANDLE;
                }

                if (device) {
                    vkDestroyDevice(device, nullptr);
                    device = VK_NULL_HANDLE;
                }
                if (surface) {
                    vkDestroySurfaceKHR(instance, surface, nullptr);
                    surface = VK_NULL_HANDLE;
                }
                if (debugMessenger) {
                    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
                    debugMessenger = VK_NULL_HANDLE;
                }
                if (instance) {
                    vkDestroyInstance(instance, nullptr);
                    instance = VK_NULL_HANDLE;
                }

                if (shadercCompiler) {
                    shaderc_compiler_release(static_cast<shaderc_compiler_t>(shadercCompiler));
                    shadercCompiler = nullptr;
                }

                physicalDevice = VK_NULL_HANDLE;
                graphicsQueue = VK_NULL_HANDLE;
                presentQueue = VK_NULL_HANDLE;
                window = nullptr;
                initialized = false;
                nextId = 1;
            }

            void VulkanRenderDevice::MakeCurrent() {}

            void VulkanRenderDevice::SetVSync(bool enabled)
            {
                if (vSyncEnabled == enabled) return;
                vSyncEnabled = enabled;
                if (initialized) RecreateSwapchain();
            }

            // ---------------------------------------------------------------------------
            // Present / deferred draw replay
            // ---------------------------------------------------------------------------

            void VulkanRenderDevice::Present()
            {
                if (!initialized) {
                    return;
                }
                if (swapchain == VK_NULL_HANDLE) {
                    RecreateSwapchain();
                    pendingDraws.clear();
                    pendingClearMask = ClearMask::None;
                    pendingImGuiDrawData = nullptr;
                    return;
                }

                const VkFence currentFence = inFlightFences[currentFrame];
                vkWaitForFences(device, 1, &currentFence, VK_TRUE, UINT64_MAX);
                RetireOrphanedBuffers();

                std::uint32_t imageIndex = 0;
                const VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                    imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

                if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
                    RecreateSwapchain();
                    pendingDraws.clear();
                    pendingClearMask = ClearMask::None;
                    pendingImGuiDrawData = nullptr;
                    return;
                }
                if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
                    RTB_ERROR("VulkanRenderDevice: vkAcquireNextImageKHR failed");
                    pendingDraws.clear();
                    pendingImGuiDrawData = nullptr;
                    return;
                }

                if (imageIndex < imagesInFlight.size() && imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                    vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
                }
                imagesInFlight[imageIndex] = currentFence;

                if (descriptorPools[currentFrame]) {
                    vkResetDescriptorPool(device, descriptorPools[currentFrame], 0);
                }

                const VkCommandBuffer cmd = commandBuffers[currentFrame];
                vkResetCommandBuffer(cmd, 0);

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkBeginCommandBuffer failed");
                    pendingDraws.clear();
                    pendingImGuiDrawData = nullptr;
                    return;
                }

                bool inPass = false;
                GpuId activeTarget = kInvalidGpuId;
                bool touchedSwapchain = false;

                const std::uint32_t drawCount = static_cast<std::uint32_t>(
                    std::min<std::size_t>(pendingDraws.size(), kMaxDrawsPerFrame));

                for (std::uint32_t di = 0; di < drawCount; ++di) {
                    const DrawCommand& draw = pendingDraws[di];
                    auto progIt = programs.find(draw.program);
                    auto vaoIt = vaos.find(draw.vao);
                    if (progIt == programs.end() || !progIt->second.valid
                        || vaoIt == vaos.end()) {
                                                continue;
                    }

                    if (!inPass || activeTarget != draw.targetFramebuffer) {
                        EndActiveRenderPass(cmd, inPass, activeTarget);
                        if (!BeginTargetRenderPass(cmd, draw.targetFramebuffer, imageIndex, clearColor, inPass, activeTarget)) {
                                                        continue;
                        }
                        if (activeTarget == 0) {
                            touchedSwapchain = true;
                        }
                    }

                    ReplayDraw(cmd, draw, di);
                }

                if (pendingImGuiDrawData) {
                    if (!inPass || activeTarget != 0) {
                        EndActiveRenderPass(cmd, inPass, activeTarget);
                        BeginTargetRenderPass(cmd, 0, imageIndex, clearColor, inPass, activeTarget);
                    }
                    touchedSwapchain = true;
                    ImGui_ImplVulkan_RenderDrawData(pendingImGuiDrawData, cmd);
                    pendingImGuiDrawData = nullptr;
                }

                if (!touchedSwapchain) {
                    // Ensure the swapchain image is always cleared + transitioned to a
                    // presentable layout even on frames with no draws targeting it.
                    EndActiveRenderPass(cmd, inPass, activeTarget);
                    BeginTargetRenderPass(cmd, 0, imageIndex, clearColor, inPass, activeTarget);
                }

                EndActiveRenderPass(cmd, inPass, activeTarget);

                if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkEndCommandBuffer failed");
                    pendingDraws.clear();
                    pendingClearMask = ClearMask::None;
                    return;
                }

                const VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
                const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                const VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cmd;
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = signalSemaphores;

                vkResetFences(device, 1, &currentFence);

                const VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, currentFence);
                if (submitResult != VK_SUCCESS) {
                    RTB_ERROR(std::string("VulkanRenderDevice: vkQueueSubmit failed (")
                        + std::to_string(static_cast<int>(submitResult)) + ")");
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

                if (!pendingOrphans.empty()) {
                    auto& slot = orphanedBuffersByFrame[currentFrame];
                    slot.insert(slot.end(), pendingOrphans.begin(), pendingOrphans.end());
                    pendingOrphans.clear();
                }
                if (!pendingTextureOrphans.empty()) {
                    auto& slot = orphanedTexturesByFrame[currentFrame];
                    slot.insert(slot.end(), pendingTextureOrphans.begin(), pendingTextureOrphans.end());
                    pendingTextureOrphans.clear();
                }
                pendingDraws.clear();
                pendingClearMask = ClearMask::None;
                FlushDeferredResourceDestroys();
                currentFrame = (currentFrame + 1) % static_cast<std::size_t>(kMaxFramesInFlight);
            }

            bool VulkanRenderDevice::BeginTargetRenderPass(VkCommandBuffer cmd, GpuId target, std::uint32_t swapImageIndex,
                                                           float clearCol[4], bool& inPass, GpuId& activeTarget)
            {
                if (target == 0) {
                    std::array<VkClearValue, 2> clearValues{};
                    clearValues[0].color = { { clearCol[0], clearCol[1], clearCol[2], clearCol[3] } };
                    clearValues[1].depthStencil = { 1.0f, 0 };

                    VkRenderPassBeginInfo rpInfo{};
                    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    rpInfo.renderPass = renderPass;
                    rpInfo.framebuffer = swapchainFramebuffers[swapImageIndex];
                    rpInfo.renderArea.offset = { 0, 0 };
                    rpInfo.renderArea.extent = swapchainExtent;
                    rpInfo.clearValueCount = 2;
                    rpInfo.pClearValues = clearValues.data();
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

                    inPass = true;
                    activeTarget = 0;
                    return true;
                }

                auto it = framebuffers.find(target);
                if (it == framebuffers.end() || !it->second.complete
                    || !it->second.framebuffer || !it->second.renderPass) {
                    return false;
                }
                FramebufferResource& fb = it->second;

                VkRenderPassBeginInfo rpInfo{};
                rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                rpInfo.renderPass = fb.renderPass;
                rpInfo.framebuffer = fb.framebuffer;
                rpInfo.renderArea.offset = { 0, 0 };
                rpInfo.renderArea.extent = { static_cast<std::uint32_t>(fb.width), static_cast<std::uint32_t>(fb.height) };

                if (fb.depthOnly) {
                    std::array<VkClearValue, 1> clearValues{};
                    clearValues[0].depthStencil = { 1.0f, 0 };
                    rpInfo.clearValueCount = 1;
                    rpInfo.pClearValues = clearValues.data();
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
                }
                else if (fb.colorOnlyLoad) {
                    // LOAD_OP_LOAD — clear values unused but Vulkan still wants a count match.
                    std::array<VkClearValue, 1> clearValues{};
                    clearValues[0].color = { { 0.f, 0.f, 0.f, 1.f } };
                    rpInfo.clearValueCount = 1;
                    rpInfo.pClearValues = clearValues.data();
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
                }
                else {
                    std::array<VkClearValue, 2> clearValues{};
                    clearValues[0].color = { { fb.clearColor[0], fb.clearColor[1], fb.clearColor[2], fb.clearColor[3] } };
                    clearValues[1].depthStencil = { 1.0f, 0 };
                    rpInfo.clearValueCount = 2;
                    rpInfo.pClearValues = clearValues.data();
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
                }

                inPass = true;
                activeTarget = target;
                return true;
            }

            void VulkanRenderDevice::EndActiveRenderPass(VkCommandBuffer cmd, bool& inPass, GpuId endingTarget)
            {
                if (!inPass) {
                    return;
                }
                vkCmdEndRenderPass(cmd);
                inPass = false;

                // Ensure shadow-map depth writes are visible to later fragment sampling.
                if (endingTarget != 0 && endingTarget != kInvalidGpuId) {
                    auto fbIt = framebuffers.find(endingTarget);
                    if (fbIt != framebuffers.end() && fbIt->second.depthOnly) {
                        auto texIt = textures.find(fbIt->second.depthTexture);
                        if (texIt != textures.end() && texIt->second.image) {
                            VkImageMemoryBarrier barrier{};
                            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                            barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                            barrier.image = texIt->second.image;
                            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                            barrier.subresourceRange.levelCount = 1;
                            barrier.subresourceRange.layerCount = 1;
                            vkCmdPipelineBarrier(cmd,
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr, 1, &barrier);
                        }
                    }
                }
            }

            VkRenderPass VulkanRenderDevice::ResolveRenderPassForTarget(GpuId targetFramebuffer) const
            {
                if (targetFramebuffer == 0) {
                    return renderPass;
                }
                auto it = framebuffers.find(targetFramebuffer);
                if (it != framebuffers.end() && it->second.renderPass) {
                    return it->second.renderPass;
                }
                return renderPass;
            }

            void VulkanRenderDevice::ReplayDraw(VkCommandBuffer cmd, const DrawCommand& draw, std::uint32_t drawSlot)
            {
                auto progIt = programs.find(draw.program);
                auto vaoIt = vaos.find(draw.vao);
                if (progIt == programs.end() || !progIt->second.valid
                    || vaoIt == vaos.end()) {
                    return;
                }

                VkPipeline pipeline = GetOrCreatePipeline(draw);
                if (!pipeline) {
                    return;
                }
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                std::uint32_t targetWidth = swapchainExtent.width;
                std::uint32_t targetHeight = swapchainExtent.height;
                if (draw.targetFramebuffer != 0) {
                    auto fbIt = framebuffers.find(draw.targetFramebuffer);
                    if (fbIt != framebuffers.end()) {
                        targetWidth = static_cast<std::uint32_t>(fbIt->second.width);
                        targetHeight = static_cast<std::uint32_t>(fbIt->second.height);
                    }
                }

                int vpX = draw.viewport[0];
                int vpY = draw.viewport[1];
                int vpW = draw.viewport[2] > 0 ? draw.viewport[2] : static_cast<int>(targetWidth);
                int vpH = draw.viewport[3] > 0 ? draw.viewport[3] : static_cast<int>(targetHeight);
                                // Vulkan clip Y is down; engine projection is OpenGL-style (Y up). Prefer a
                // positive viewport + ImGui UV flip (same as the OpenGL editor path) instead of
                // a negative viewport height, which double-flips with ImGui and confuses some paths.
                VkViewport viewport{};
                viewport.x = static_cast<float>(vpX);
                viewport.y = static_cast<float>(vpY);
                viewport.width = static_cast<float>(vpW);
                viewport.height = static_cast<float>(vpH);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { std::max(0, vpX), std::max(0, vpY) };
                scissor.extent.width = static_cast<std::uint32_t>(std::max(0, vpW));
                scissor.extent.height = static_cast<std::uint32_t>(std::max(0, vpH));
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                // Upload PerDraw slot
                const VkDeviceSize slotOffset = static_cast<VkDeviceSize>(drawSlot) * perDrawSlotStride;
                void* perDrawBase = perDrawMapped[currentFrame];
                if (perDrawBase) {
                    std::memcpy(static_cast<char*>(perDrawBase) + slotOffset, &draw.perDraw, sizeof(PerDrawCPU));
                }

                // Vertex / index buffers â€” prefer record-time VkBuffer snapshots so later
                // SetArrayBufferData orphans cannot rewrite geometry for earlier draws.
                const VaoResource& vao = vaoIt->second;
                std::vector<VkBuffer> vbuffers;
                std::vector<VkDeviceSize> offsets;
                if (!draw.vkVertexBuffers.empty()) {
                    vbuffers.reserve(draw.vkVertexBuffers.size() + 1);
                    offsets.reserve(draw.vkVertexBuffers.size() + 1);
                    for (VkBuffer vb : draw.vkVertexBuffers) {
                        if (!vb) {
                            return;
                        }
                        NoteBufferInFlight(vb);
                        vbuffers.push_back(vb);
                        offsets.push_back(0);
                    }
                }
                else {
                    const std::vector<GpuId> bufferOrder = ComputeVaoBufferOrder(vao);
                    vbuffers.reserve(bufferOrder.size() + 1);
                    offsets.reserve(bufferOrder.size() + 1);
                    for (GpuId bid : bufferOrder) {
                        auto bit = buffers.find(bid);
                        if (bit == buffers.end() || !bit->second.buffer) {
                            return;
                        }
                        NoteBufferInFlight(bit->second.buffer);
                        vbuffers.push_back(bit->second.buffer);
                        offsets.push_back(0);
                    }
                }
                // Dummy binding for shader inputs the VAO does not provide (e.g. instance
                // matrices on non-instanced draws). Matches GetOrCreatePipeline.
                if (fallbackVertexBuffer) {
                    vbuffers.push_back(fallbackVertexBuffer);
                    offsets.push_back(0);
                }
                if (!vbuffers.empty()) {
                    vkCmdBindVertexBuffers(cmd, 0, static_cast<std::uint32_t>(vbuffers.size()),
                        vbuffers.data(), offsets.data());
                }

                if (draw.indexed) {
                    VkBuffer indexBuffer = draw.vkIndexBuffer;
                    if (!indexBuffer) {
                        auto eit = buffers.find(vao.elementBuffer);
                        if (eit != buffers.end()) {
                            indexBuffer = eit->second.buffer;
                        }
                    }
                    if (!indexBuffer) {
                        return;
                    }
                    NoteBufferInFlight(indexBuffer);
                    const VkIndexType idxType = (draw.indexType == IndexType::UInt16)
                        ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, idxType);
                }

                // Descriptor set
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = descriptorPools[currentFrame];
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &descriptorSetLayout;
                VkDescriptorSet dset = VK_NULL_HANDLE;
                if (vkAllocateDescriptorSets(device, &allocInfo, &dset) != VK_SUCCESS) {
                    return;
                }

                auto resolveTex = [&](GpuId id, bool cube) -> const TextureResource& {
                    auto it = textures.find(id);
                    if (it != textures.end() && it->second.view && it->second.sampler) {
                        return it->second;
                    }
                    return cube ? fallbackCubemap : fallbackTexture2D;
                };

                VkDescriptorBufferInfo uboInfos[4]{};
                uboInfos[0].buffer = draw.vkLighting ? draw.vkLighting : fallbackUBOBuffer;
                uboInfos[0].range = VK_WHOLE_SIZE;
                uboInfos[1].buffer = draw.vkCamera ? draw.vkCamera : fallbackUBOBuffer;
                uboInfos[1].range = VK_WHOLE_SIZE;
                uboInfos[2].buffer = draw.vkBone ? draw.vkBone : fallbackUBOBuffer;
                uboInfos[2].range = VK_WHOLE_SIZE;
                uboInfos[3].buffer = perDrawBuffers[currentFrame];
                uboInfos[3].offset = slotOffset;
                uboInfos[3].range = sizeof(PerDrawCPU);
                NoteBufferInFlight(uboInfos[0].buffer);
                NoteBufferInFlight(uboInfos[1].buffer);
                NoteBufferInFlight(uboInfos[2].buffer);

                const auto t0It = textures.find(draw.texSlot0);
                const bool tex0Missing = (draw.texSlot0 != kInvalidGpuId)
                    && (t0It == textures.end() || !t0It->second.view);
                const TextureResource& t0 = resolveTex(draw.texSlot0, false);
                const TextureResource& t1 = resolveTex(draw.texSlot1, false);
                const TextureResource& tc = resolveTex(draw.cubemapSlot0, true);
                const TextureResource& ddgiIrr = resolveTex(draw.ddgiIrradiance, false);
                const TextureResource& ddgiDist = resolveTex(draw.ddgiDistance, false);

                VkDescriptorImageInfo imgInfos[5]{};
                imgInfos[0].imageLayout = t0.isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgInfos[0].imageView = t0.view;
                imgInfos[0].sampler = t0.sampler;
                imgInfos[1].imageLayout = t1.isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgInfos[1].imageView = t1.view;
                imgInfos[1].sampler = t1.sampler;
                imgInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgInfos[2].imageView = tc.view;
                imgInfos[2].sampler = tc.sampler;
                imgInfos[3].imageLayout = (ddgiIrr.currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                    ? ddgiIrr.currentLayout
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgInfos[3].imageView = ddgiIrr.view;
                imgInfos[3].sampler = ddgiIrr.sampler;
                imgInfos[4].imageLayout = (ddgiDist.currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                    ? ddgiDist.currentLayout
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imgInfos[4].imageView = ddgiDist.view;
                imgInfos[4].sampler = ddgiDist.sampler;

                VkWriteDescriptorSet writes[10]{};
                for (int i = 0; i < 4; ++i) {
                    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[i].dstSet = dset;
                    writes[i].dstBinding = static_cast<std::uint32_t>(i);
                    writes[i].descriptorCount = 1;
                    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writes[i].pBufferInfo = &uboInfos[i];
                }
                // PerDraw is dynamic offset style via offset in buffer info (static UBO with offset).
                // Using UNIFORM_BUFFER with non-zero offset is valid.
                for (int i = 0; i < 3; ++i) {
                    writes[4 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[4 + i].dstSet = dset;
                    writes[4 + i].dstBinding = 4 + static_cast<std::uint32_t>(i);
                    writes[4 + i].descriptorCount = 1;
                    writes[4 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[4 + i].pImageInfo = &imgInfos[i];
                }
                VkDescriptorBufferInfo ddgiUboInfo{};
                ddgiUboInfo.buffer = ResolveBufferHandle(draw.ddgiUBO);
                if (!ddgiUboInfo.buffer) ddgiUboInfo.buffer = fallbackUBOBuffer;
                ddgiUboInfo.offset = 0;
                ddgiUboInfo.range = VK_WHOLE_SIZE;
                writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[7].dstSet = dset;
                writes[7].dstBinding = 7;
                writes[7].descriptorCount = 1;
                writes[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                writes[7].pBufferInfo = &ddgiUboInfo;
                for (int i = 0; i < 2; ++i) {
                    writes[8 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[8 + i].dstSet = dset;
                    writes[8 + i].dstBinding = 8 + static_cast<std::uint32_t>(i);
                    writes[8 + i].descriptorCount = 1;
                    writes[8 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writes[8 + i].pImageInfo = &imgInfos[3 + i];
                }
                vkUpdateDescriptorSets(device, 10, writes, 0, nullptr);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                    0, 1, &dset, 0, nullptr);

                if (draw.indexed) {
                    if (draw.instanceCount > 1) {
                        vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(draw.count),
                            static_cast<std::uint32_t>(draw.instanceCount), 0, 0, 0);
                    }
                    else {
                        vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(draw.count), 1, 0, 0, 0);
                    }
                }
                else {
                    if (draw.instanceCount > 1) {
                        vkCmdDraw(cmd, static_cast<std::uint32_t>(draw.count),
                            static_cast<std::uint32_t>(draw.instanceCount),
                            static_cast<std::uint32_t>(draw.first), 0);
                    }
                    else {
                        vkCmdDraw(cmd, static_cast<std::uint32_t>(draw.count), 1,
                            static_cast<std::uint32_t>(draw.first), 0);
                    }
                }
                }

            // ---------------------------------------------------------------------------
            // Raster state
            // ---------------------------------------------------------------------------

            void VulkanRenderDevice::SetViewport(int x, int y, int width, int height)
            {
                pendingViewport[0] = x;
                pendingViewport[1] = y;
                pendingViewport[2] = width;
                pendingViewport[3] = height;
            }

            void VulkanRenderDevice::SetClearColor(float r, float g, float b, float a)
            {
                clearColor[0] = r; clearColor[1] = g; clearColor[2] = b; clearColor[3] = a;
            }

            void VulkanRenderDevice::Clear(ClearMask mask)
            {
                if (currentBoundFramebuffer != 0) {
                    auto it = framebuffers.find(currentBoundFramebuffer);
                    if (it != framebuffers.end()) {
                        it->second.pendingClearMask = it->second.pendingClearMask | mask;
                        it->second.clearColor[0] = clearColor[0];
                        it->second.clearColor[1] = clearColor[1];
                        it->second.clearColor[2] = clearColor[2];
                        it->second.clearColor[3] = clearColor[3];
                    }
                }
                else {
                    pendingClearMask = pendingClearMask | mask;
                }
            }

            void VulkanRenderDevice::SetDepthTest(bool enabled) { stateDepthTest = enabled; }
            void VulkanRenderDevice::SetDepthFunc(DepthFunc func) { stateDepthFunc = func; }
            void VulkanRenderDevice::SetDepthWrite(bool enabled) { stateDepthWrite = enabled; }
            void VulkanRenderDevice::SetCullFace(bool enabled) { stateCullFace = enabled; }
            void VulkanRenderDevice::SetBlend(bool enabled) { stateBlend = enabled; }
            void VulkanRenderDevice::SetBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha)
            {
                stateSrcRGB = srcRGB; stateDstRGB = dstRGB;
                stateSrcAlpha = srcAlpha; stateDstAlpha = dstAlpha;
            }
            void VulkanRenderDevice::SetColorMask(bool r, bool g, bool b, bool a)
            {
                stateColorMask[0] = r; stateColorMask[1] = g;
                stateColorMask[2] = b; stateColorMask[3] = a;
            }

            // ---------------------------------------------------------------------------
            // Shaders / uniforms
            // ---------------------------------------------------------------------------

            const VulkanRenderDevice::PerDrawField* VulkanRenderDevice::FindPerDrawField(const char* name)
            {
                static const PerDrawField kFields[] = {
                    { "uModel", "mat4", static_cast<int>(offsetof(PerDrawCPU, uModel)), false },
                    { "uLightSpaceMatrix", "mat4", static_cast<int>(offsetof(PerDrawCPU, uLightSpaceMatrix)), false },
                    { "uViewProjection", "mat4", static_cast<int>(offsetof(PerDrawCPU, uViewProjection)), false },
                    { "uColor", "vec4", static_cast<int>(offsetof(PerDrawCPU, uColor)), false },
                    { "uDiffuseColor", "vec3", static_cast<int>(offsetof(PerDrawCPU, uDiffuseColor)), false },
                    { "uShininess", "float", static_cast<int>(offsetof(PerDrawCPU, uShininess)), false },
                    { "uWorldPosition", "vec3", static_cast<int>(offsetof(PerDrawCPU, uWorldPosition)), false },
                    { "uVerticalOffset", "float", static_cast<int>(offsetof(PerDrawCPU, uVerticalOffset)), false },
                    { "uSize", "vec2", static_cast<int>(offsetof(PerDrawCPU, uSize)), false },
                    { "uFrame", "float", static_cast<int>(offsetof(PerDrawCPU, uFrame)), false },
                    { "uShadowBias", "float", static_cast<int>(offsetof(PerDrawCPU, uShadowBias)), false },
                    { "uHasTexture", "bool", static_cast<int>(offsetof(PerDrawCPU, uHasTexture)), true },
                    { "uUseInstancing", "bool", static_cast<int>(offsetof(PerDrawCPU, uUseInstancing)), true },
                    { "uHasAnimation", "bool", static_cast<int>(offsetof(PerDrawCPU, uHasAnimation)), true },
                    { "uUseInstanceColor", "bool", static_cast<int>(offsetof(PerDrawCPU, uUseInstanceColor)), true },
                    { "uHasShadows", "bool", static_cast<int>(offsetof(PerDrawCPU, uHasShadows)), true },
                    { "uSheetEnabled", "bool", static_cast<int>(offsetof(PerDrawCPU, uSheetEnabled)), true },
                    { "uSheetColumns", "int", static_cast<int>(offsetof(PerDrawCPU, uSheetColumns)), false },
                    { "uSheetRows", "int", static_cast<int>(offsetof(PerDrawCPU, uSheetRows)), false },
                    { "uSheetFrameCount", "int", static_cast<int>(offsetof(PerDrawCPU, uSheetFrameCount)), false },
                    { "uTime", "float", static_cast<int>(offsetof(PerDrawCPU, uTime)), false },
                    { "uPulseSpeed", "float", static_cast<int>(offsetof(PerDrawCPU, uPulseSpeed)), false },
                    { "uGlowIntensity", "float", static_cast<int>(offsetof(PerDrawCPU, uGlowIntensity)), false },
                    { "uFogEnabled", "bool", static_cast<int>(offsetof(PerDrawCPU, uFogEnabled)), true },
                    { "uFogDensity", "float", static_cast<int>(offsetof(PerDrawCPU, uFogDensity)), false },
                    { "uFogHeight", "float", static_cast<int>(offsetof(PerDrawCPU, uFogHeight)), false },
                    { "uFogHeightFalloff", "float", static_cast<int>(offsetof(PerDrawCPU, uFogHeightFalloff)), false },
                    { "uFogColor", "vec3", static_cast<int>(offsetof(PerDrawCPU, uFogColor)), false },
                    { "uFogStart", "float", static_cast<int>(offsetof(PerDrawCPU, uFogStart)), false },
                    { "uFogEnd", "float", static_cast<int>(offsetof(PerDrawCPU, uFogEnd)), false },
                    { "uVolumetricFogEnabled", "bool", static_cast<int>(offsetof(PerDrawCPU, uVolumetricFogEnabled)), true },
                    { "uVolumetricIntensity", "float", static_cast<int>(offsetof(PerDrawCPU, uVolumetricIntensity)), false },
                    { "uVolumetricAnisotropy", "float", static_cast<int>(offsetof(PerDrawCPU, uVolumetricAnisotropy)), false },
                    { "uVolumetricSamples", "int", static_cast<int>(offsetof(PerDrawCPU, uVolumetricSamples)), false },
                    { "uCameraNear", "float", static_cast<int>(offsetof(PerDrawCPU, uCameraNear)), false },
                    { "uCameraFar", "float", static_cast<int>(offsetof(PerDrawCPU, uCameraFar)), false },
                    { "uDepthZeroToOne", "bool", static_cast<int>(offsetof(PerDrawCPU, uDepthZeroToOne)), true },
                    { "uVolumetricMaxLuminance", "float", static_cast<int>(offsetof(PerDrawCPU, uVolumetricMaxLuminance)), false },
                    // Sampler uniforms map to no PerDraw field; return sentinel offsets for GetUniformLocation
                    { "uTexture", "sampler", -100, false },
                    { "uShadowMap", "sampler", -101, false },
                    { "uSkybox", "sampler", -102, false },
                    { "uDiffuse", "sampler", -100, false },
                    { "uSceneDepth", "sampler", -100, false },
                };
                if (!name) return nullptr;
                for (const auto& f : kFields) {
                    if (std::strcmp(f.name, name) == 0) return &f;
                }
                return nullptr;
            }

            void* VulkanRenderDevice::PerDrawFieldPtr(int location)
            {
                if (location < 0 || location >= static_cast<int>(sizeof(PerDrawCPU))) {
                    return nullptr;
                }
                return reinterpret_cast<char*>(&currentPerDraw) + location;
            }

            std::string VulkanRenderDevice::TransformShaderSource(const std::string& source, bool isFragment) const
            {
                std::string src = source;
                src = std::regex_replace(src, std::regex(R"(#version\s+\d+(\s+core)?)"), "#version 450");

                // Add set=0 to existing std140 UBO bindings.
                src = std::regex_replace(src,
                    std::regex(R"(layout\s*\(\s*std140\s*,\s*binding\s*=\s*(\d+)\s*\))"),
                    "layout(std140, set = 0, binding = $1)");

                // Blocks that only had layout(std140) (no binding) â€” CameraData-style â†’ binding 1.
                src = std::regex_replace(src,
                    std::regex(R"(layout\s*\(\s*std140\s*\)\s*uniform)"),
                    "layout(std140, set = 0, binding = 1) uniform");

                // Strip loose uniforms that we inject via PerDraw / sampler bindings.
                for (const char* name : kLooseUniformNames) {
                    std::string pattern = std::string(R"(uniform\s+[\w]+\s+)") + name + R"(\s*;)";
                    src = std::regex_replace(src, std::regex(pattern), "");
                }

                // Strip any remaining non-opaque / sampler loose uniforms (Vulkan forbids them).
                // Known ones are already in PerDraw; unknowns become compile errors if still referenced.
                src = std::regex_replace(src,
                    std::regex(R"(^\s*uniform\s+(?!sampler)[\w]+\s+[\w]+\s*;\s*$)", std::regex::multiline),
                    "");
                src = std::regex_replace(src,
                    std::regex(R"(^\s*uniform\s+sampler[\w]*\s+[\w]+\s*;\s*$)", std::regex::multiline),
                    "");

                // Assign locations to stage IO without layout qualifiers.
                {
                    std::istringstream in(src);
                    std::ostringstream out;
                    std::string line;
                    int nextLoc = 0;
                    const char* ioKeyword = isFragment ? "in" : "out";
                    std::regex ioRe(std::string(R"(^\s*)") + ioKeyword + R"(\s+([\w]+)\s+([\w]+)\s*;)");
                    std::regex fragOutRe(R"(^\s*out\s+([\w]+)\s+([\w]+)\s*;)");
                    while (std::getline(in, line)) {
                        std::smatch m;
                        if (isFragment && std::regex_match(line, m, fragOutRe)) {
                            out << "layout(location = 0) out " << m[1].str() << " " << m[2].str() << ";\n";
                            continue;
                        }
                        if (std::regex_match(line, m, ioRe)) {
                            out << "layout(location = " << nextLoc++ << ") " << ioKeyword << " "
                                << m[1].str() << " " << m[2].str() << ";\n";
                            continue;
                        }
                        out << line << "\n";
                    }
                    src = out.str();
                }

                // Inject PerDraw + samplers after #version line.
                const auto verPos = src.find("#version");
                std::size_t insertAt = 0;
                if (verPos != std::string::npos) {
                    insertAt = src.find('\n', verPos);
                    if (insertAt == std::string::npos) insertAt = src.size();
                    else ++insertAt;
                }
                const std::string inject = std::string("\n") + kPerDrawBlockGlsl + kSamplerBindingsGlsl + "\n";
                src.insert(insertAt, inject);

                // Vulkan path: albedo is sampled as linear (SRGB formats) but offscreen FBO +
                // ImGui display are UNORM without automatic encode. Apply gamma so Game/Scene
                // View match the OpenGL appearance on an sRGB monitor.
                if (isFragment && src.find("ShadowCalculation") != std::string::npos) {
                    const std::string from =
                        "    // Transform from [-1,1] to [0,1] range\n"
                        "    projCoords = projCoords * 0.5 + 0.5;\n";
                    // Light matrix includes VulkanClipCorrection (Z remap only).
                    // XY still need NDC->UV; Z is already in [0,1].
                    const std::string to =
                        "    // Vulkan: XY NDC[-1,1]->UV; Z already [0,1] via clip correction\n"
                        "    projCoords.xy = projCoords.xy * 0.5 + 0.5;\n"
                        "    float _shadowBias = bias;\n";
                    const auto pos = src.find(from);
                    if (pos != std::string::npos) {
                        src.replace(pos, from.size(), to);
                        // Apply uShadowBias (basic.frag declared it but never used it).
                        const std::string depthFrom = "float currentDepth = projCoords.z - slopeBias;";
                        const std::string depthTo =
                            "float currentDepth = projCoords.z - slopeBias - _shadowBias;";
                        const auto dpos = src.find(depthFrom);
                        if (dpos != std::string::npos) {
                            src.replace(dpos, depthFrom.size(), depthTo);
                        }
                        // Soften point-light specular streaks (torch/floor rays).
                        const std::string specFrom =
                            "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);\n"
                            "    vec3 specular = spec * light.color * light.intensity * 0.5;\n"
                            "    \n"
                            "    return (diffuse + specular) * attenuation;";
                        const std::string specTo =
                            "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);\n"
                            "    vec3 specular = spec * light.color * light.intensity * 0.04;\n"
                            "    \n"
                            "    return (diffuse + specular) * attenuation;";
                        // Only the first CalcPointLight block (before CalcSpotLight).
                        const auto spos = src.find(specFrom);
                        if (spos != std::string::npos) {
                            src.replace(spos, specFrom.size(), specTo);
                        }
                    }
                }

                if (isFragment && src.find("CalcDirectionalLight") != std::string::npos) {
                    const std::string from =
                        "FragColor = vec4(litColor * tint + flashAdd, texColor.a * effectiveColor.a);";
                    // Reinhard + gamma: linear HDR-ish lighting â†’ display-referred UNORM FBO.
                    const std::string to =
                        "vec3 _linOut = max(litColor * tint + flashAdd, vec3(0.0));\n"
                        "    _linOut = _linOut / (_linOut + vec3(1.0));\n"
                        "    FragColor = vec4(pow(_linOut, vec3(1.0 / 2.2)), texColor.a * effectiveColor.a);";
                    const auto pos = src.find(from);
                    if (pos != std::string::npos) {
                        src.replace(pos, from.size(), to);
                    }
                }
                return src;
            }

            std::vector<unsigned int> VulkanRenderDevice::ParseVertexInputLocations(const std::string& vertexSource)
            {
                std::vector<unsigned int> locs;
                static const std::regex re(
                    R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\b)");
                for (std::sregex_iterator it(vertexSource.begin(), vertexSource.end(), re), end; it != end; ++it) {
                    const unsigned int loc = static_cast<unsigned int>(std::stoul((*it)[1].str()));
                    if (std::find(locs.begin(), locs.end(), loc) == locs.end()) {
                        locs.push_back(loc);
                    }
                }
                return locs;
            }

            GpuId VulkanRenderDevice::CreateShaderProgram(const std::string& vertexSource,
                                                          const std::string& fragmentSource)
            {
                const GpuId id = nextId++;
                ShaderProgramResource prog{};
                prog.usedVertexLocations = ParseVertexInputLocations(vertexSource);

                auto compile = [&](const std::string& glsl, shaderc_shader_kind kind, const char* name) -> VkShaderModule {
                    const std::string transformed = TransformShaderSource(glsl, kind == shaderc_fragment_shader);
                    shaderc_compile_options_t options = shaderc_compile_options_initialize();
                    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan,
                        shaderc_env_version_vulkan_1_1);
                    shaderc_compile_options_set_source_language(options, shaderc_source_language_glsl);
                    shaderc_compilation_result_t result = shaderc_compile_into_spv(
                        static_cast<shaderc_compiler_t>(shadercCompiler),
                        transformed.c_str(), transformed.size(),
                        kind, name, "main", options);
                    shaderc_compile_options_release(options);
                    if (!result) {
                        RTB_ERROR(std::string("VulkanRenderDevice: shaderc returned null for ") + name);
                        return VK_NULL_HANDLE;
                    }
                    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
                        RTB_ERROR(std::string("Vulkan SPIR-V compile failed (") + name + "): "
                            + shaderc_result_get_error_message(result));
                        RTB_ERROR(std::string("Transformed source preview:\n") + transformed.substr(0, 1200));
                        shaderc_result_release(result);
                        return VK_NULL_HANDLE;
                    }
                    const char* bytes = shaderc_result_get_bytes(result);
                    const size_t len = shaderc_result_get_length(result);
                    VkShaderModuleCreateInfo ci{};
                    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                    ci.codeSize = len;
                    ci.pCode = reinterpret_cast<const std::uint32_t*>(bytes);
                    VkShaderModule module = VK_NULL_HANDLE;
                    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
                        RTB_ERROR(std::string("VulkanRenderDevice: vkCreateShaderModule failed for ") + name);
                        module = VK_NULL_HANDLE;
                    }
                    shaderc_result_release(result);
                    return module;
                };

                prog.vertModule = compile(vertexSource, shaderc_vertex_shader, "vert.glsl");
                prog.fragModule = compile(fragmentSource, shaderc_fragment_shader, "frag.glsl");
                prog.valid = prog.vertModule != VK_NULL_HANDLE && prog.fragModule != VK_NULL_HANDLE;
                if (!prog.valid) {
                    if (prog.vertModule) vkDestroyShaderModule(device, prog.vertModule, nullptr);
                    if (prog.fragModule) vkDestroyShaderModule(device, prog.fragModule, nullptr);
                    prog = {};
                    RTB_WARN("VulkanRenderDevice: CreateShaderProgram produced an invalid program (draws will skip it)");
                } else {
                }
                programs[id] = prog;
                return id;
            }

            void VulkanRenderDevice::DestroyShaderProgram(GpuId program)
            {
                auto it = programs.find(program);
                if (it == programs.end()) return;
                DestroyAllPipelines(); // pipelines reference modules
                if (it->second.vertModule) vkDestroyShaderModule(device, it->second.vertModule, nullptr);
                if (it->second.fragModule) vkDestroyShaderModule(device, it->second.fragModule, nullptr);
                programs.erase(it);
                if (currentProgram == program) currentProgram = kInvalidGpuId;
            }

            void VulkanRenderDevice::BindShaderProgram(GpuId program)
            {
                currentProgram = program;
            }

            int VulkanRenderDevice::GetUniformLocation(GpuId /*program*/, const char* name)
            {
                const PerDrawField* field = FindPerDrawField(name);
                if (!field) return -1;
                return field->offset; // sampler sentinels are negative
            }

            void VulkanRenderDevice::SetUniformBool(int location, bool value)
            {
                if (auto* p = static_cast<std::int32_t*>(PerDrawFieldPtr(location))) *p = value ? 1 : 0;
            }
            void VulkanRenderDevice::SetUniformInt(int location, int value)
            {
                if (location < 0) return; // sampler slot hints ignored (engine binds textures separately)
                if (auto* p = static_cast<std::int32_t*>(PerDrawFieldPtr(location))) *p = value;
            }
            void VulkanRenderDevice::SetUniformFloat(int location, float value)
            {
                if (auto* p = static_cast<float*>(PerDrawFieldPtr(location))) *p = value;
            }
            void VulkanRenderDevice::SetUniformVec2(int location, float x, float y)
            {
                if (auto* p = static_cast<float*>(PerDrawFieldPtr(location))) { p[0] = x; p[1] = y; }
            }
            void VulkanRenderDevice::SetUniformVec3(int location, float x, float y, float z)
            {
                if (auto* p = static_cast<float*>(PerDrawFieldPtr(location))) { p[0] = x; p[1] = y; p[2] = z; }
            }
            void VulkanRenderDevice::SetUniformVec4(int location, float x, float y, float z, float w)
            {
                if (auto* p = static_cast<float*>(PerDrawFieldPtr(location))) { p[0] = x; p[1] = y; p[2] = z; p[3] = w; }
            }
            void VulkanRenderDevice::SetUniformMat4(int location, const float* matrix4x4)
            {
                if (!matrix4x4) return;
                if (auto* p = static_cast<float*>(PerDrawFieldPtr(location))) {
                    std::memcpy(p, matrix4x4, sizeof(float) * 16);
                }
            }
            void VulkanRenderDevice::BindUniformBlock(GpuId /*program*/, const char* /*blockName*/, unsigned int /*bindingPoint*/) {}

            // ---------------------------------------------------------------------------
            // Buffers / UBOs
            // ---------------------------------------------------------------------------

            GpuId VulkanRenderDevice::CreateBuffer()
            {
                const GpuId id = nextId++;
                buffers[id] = {};
                return id;
            }

            void VulkanRenderDevice::DestroyBuffer(GpuId buffer)
            {
                auto it = buffers.find(buffer);
                if (it == buffers.end()) return;
                // Pending draws snapshot VkBuffer for UBOs and resolve VBOs by GpuId at Present.
                // Never destroy/erase immediately while deferred work may still need them.
                if (!pendingDraws.empty()
                    || IsBufferInFlight(it->second.buffer)
                    || IsBufferReferencedByPendingDraws(it->second.buffer)) {
                                        deferredBufferDestroys.push_back(buffer);
                    return;
                }
                if (it->second.buffer) {
                    pendingOrphans.push_back({ it->second.buffer, it->second.memory });
                }
                buffers.erase(it);
            }

            void VulkanRenderDevice::EnsureBufferCapacity(GpuId id, std::size_t size, VkBufferUsageFlags usage)
            {
                auto it = buffers.find(id);
                if (it == buffers.end()) return;
                BufferResource& res = it->second;
                if (res.buffer && res.size >= size && (res.usage & usage) == usage) {
                    return;
                }
                if (res.buffer) {
                    // Always defer destruction â€” pending draws may still hold this VkBuffer
                    // snapshot (Bone/Camera/Lighting UBOs) even when not yet marked in-flight.
                    pendingOrphans.push_back({ res.buffer, res.memory });
                }
                res = {};
                if (size == 0) return;
                if (!CreateBufferRaw(size, usage,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        res.buffer, res.memory)) {
                    RTB_ERROR("VulkanRenderDevice: EnsureBufferCapacity failed");
                    return;
                }
                res.size = size;
                res.usage = usage;
            }

            void VulkanRenderDevice::UploadHostVisibleBuffer(VkDeviceMemory memory, const void* data, std::size_t size) const
            {
                if (!memory || !data || size == 0) return;
                void* mapped = nullptr;
                if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS) return;
                std::memcpy(mapped, data, size);
                vkUnmapMemory(device, memory);
            }

            void VulkanRenderDevice::SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage /*usage*/)
            {
                OrphanBufferIfInFlight(buffer);
                EnsureBufferCapacity(buffer, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
                auto it = buffers.find(buffer);
                if (it == buffers.end() || !it->second.memory) return;
                // In-place write is safe only when this VkBuffer is not referenced by an
                // in-flight CB. OrphanBufferIfInFlight above ensures that.
                UploadHostVisibleBuffer(it->second.memory, data, size);
            }

            void VulkanRenderDevice::SetArrayBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage /*usage*/)
            {
                // Match OpenGL: uploading array data also binds it for subsequent
                // EnableVertexAttrib* calls (Mesh::SetupMesh / UploadInstanceData).
                currentArrayBuffer = buffer;
                OrphanBufferIfInFlight(buffer);
                EnsureBufferCapacity(buffer, size,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                auto it = buffers.find(buffer);
                if (it == buffers.end() || !it->second.memory) return;
                UploadHostVisibleBuffer(it->second.memory, data, size);
            }

            void VulkanRenderDevice::SetElementBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage /*usage*/)
            {
                // Match OpenGL: ELEMENT_ARRAY_BUFFER binds are stored on the current VAO.
                currentElementBuffer = buffer;
                if (currentVAO != kInvalidGpuId) {
                    auto vaoIt = vaos.find(currentVAO);
                    if (vaoIt != vaos.end()) {
                        vaoIt->second.elementBuffer = buffer;
                        ++vaoIt->second.generation;
                    }
                }
                OrphanBufferIfInFlight(buffer);
                EnsureBufferCapacity(buffer, size,
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                auto it = buffers.find(buffer);
                if (it == buffers.end() || !it->second.memory) return;
                UploadHostVisibleBuffer(it->second.memory, data, size);
            }

            VkBuffer VulkanRenderDevice::ResolveBufferHandle(GpuId id) const
            {
                auto it = buffers.find(id);
                if (it != buffers.end() && it->second.buffer) {
                    return it->second.buffer;
                }
                return VK_NULL_HANDLE;
            }

            void VulkanRenderDevice::OrphanUniformBufferIfDeferred(GpuId buffer)
            {
                if (buffer == kInvalidGpuId || pendingDraws.empty()) {
                    return;
                }
                bool referenced = false;
                for (const DrawCommand& draw : pendingDraws) {
                    if (draw.uboLighting == buffer || draw.uboCamera == buffer || draw.uboBone == buffer) {
                        referenced = true;
                        break;
                    }
                }
                if (!referenced) {
                    return;
                }
                auto it = buffers.find(buffer);
                if (it == buffers.end() || !it->second.buffer) {
                    return;
                }
                                pendingOrphans.push_back({ it->second.buffer, it->second.memory });
                it->second.buffer = VK_NULL_HANDLE;
                it->second.memory = VK_NULL_HANDLE;
                it->second.size = 0;
                it->second.usage = 0;
            }

            void VulkanRenderDevice::RetireOrphanedBuffers()
            {
                auto& slot = orphanedBuffersByFrame[currentFrame];
                for (OrphanedBuffer& orphan : slot) {
                    if (orphan.buffer) vkDestroyBuffer(device, orphan.buffer, nullptr);
                    if (orphan.memory) vkFreeMemory(device, orphan.memory, nullptr);
                }
                slot.clear();
                buffersInFlightByFrame[currentFrame].clear();

                auto& texSlot = orphanedTexturesByFrame[currentFrame];
                for (OrphanedTexture& orphan : texSlot) {
                    if (orphan.sampler) vkDestroySampler(device, orphan.sampler, nullptr);
                    if (orphan.view) vkDestroyImageView(device, orphan.view, nullptr);
                    if (orphan.image) vkDestroyImage(device, orphan.image, nullptr);
                    if (orphan.memory) vkFreeMemory(device, orphan.memory, nullptr);
                }
                texSlot.clear();
            }

            void VulkanRenderDevice::OrphanTextureGpuResources(TextureResource& res)
            {
                if (!res.image && !res.view && !res.sampler && !res.memory) {
                    return;
                }
                pendingTextureOrphans.push_back({ res.image, res.memory, res.view, res.sampler });
                res.image = VK_NULL_HANDLE;
                res.memory = VK_NULL_HANDLE;
                res.view = VK_NULL_HANDLE;
                res.sampler = VK_NULL_HANDLE;
            }

            void VulkanRenderDevice::NoteBufferInFlight(VkBuffer buffer)
            {
                if (!buffer) return;
                auto& slot = buffersInFlightByFrame[currentFrame];
                if (std::find(slot.begin(), slot.end(), buffer) == slot.end()) {
                    slot.push_back(buffer);
                }
            }

            bool VulkanRenderDevice::IsBufferInFlight(VkBuffer buffer) const
            {
                if (!buffer) return false;
                for (const auto& slot : buffersInFlightByFrame) {
                    if (std::find(slot.begin(), slot.end(), buffer) != slot.end()) {
                        return true;
                    }
                }
                return false;
            }

            bool VulkanRenderDevice::IsBufferReferencedByPendingDraws(VkBuffer buffer) const
            {
                if (!buffer) return false;
                for (const DrawCommand& draw : pendingDraws) {
                    if (draw.vkLighting == buffer || draw.vkCamera == buffer || draw.vkBone == buffer) {
                        return true;
                    }
                    if (draw.vkIndexBuffer == buffer) {
                        return true;
                    }
                    for (VkBuffer vb : draw.vkVertexBuffers) {
                        if (vb == buffer) return true;
                    }
                }
                return false;
            }

            bool VulkanRenderDevice::IsGpuBufferUsedByPendingVaos(GpuId buffer) const
            {
                if (buffer == kInvalidGpuId || pendingDraws.empty()) return false;
                for (const DrawCommand& draw : pendingDraws) {
                    auto vaoIt = vaos.find(draw.vao);
                    if (vaoIt == vaos.end()) continue;
                    if (vaoIt->second.elementBuffer == buffer) return true;
                    for (const auto& pair : vaoIt->second.attributes) {
                        if (pair.second.buffer == buffer) return true;
                    }
                }
                return false;
            }

            void VulkanRenderDevice::FlushDeferredResourceDestroys()
            {
                if (deferredBufferDestroys.empty()
                    && deferredFramebufferDestroys.empty()
                    && deferredVaoDestroys.empty()) {
                    return;
                }
                                // Pending draws have been submitted; wait so in-flight CBs finish before teardown.
                if (device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(device);
                }
                for (GpuId id : deferredBufferDestroys) {
                    auto it = buffers.find(id);
                    if (it == buffers.end()) continue;
                    if (it->second.buffer) vkDestroyBuffer(device, it->second.buffer, nullptr);
                    if (it->second.memory) vkFreeMemory(device, it->second.memory, nullptr);
                    buffers.erase(it);
                }
                deferredBufferDestroys.clear();
                for (GpuId id : deferredFramebufferDestroys) {
                    auto it = framebuffers.find(id);
                    if (it == framebuffers.end()) continue;
                    DestroyFramebufferGpu(id, it->second);
                    framebuffers.erase(it);
                }
                deferredFramebufferDestroys.clear();
                for (GpuId id : deferredVaoDestroys) {
                    vaos.erase(id);
                }
                deferredVaoDestroys.clear();
            }

            void VulkanRenderDevice::OrphanBufferIfInFlight(GpuId buffer)
            {
                auto it = buffers.find(buffer);
                if (it == buffers.end() || !it->second.buffer) return;
                if (!IsBufferInFlight(it->second.buffer)
                    && !IsBufferReferencedByPendingDraws(it->second.buffer)
                    && !IsGpuBufferUsedByPendingVaos(buffer)) {
                    return;
                }
                                pendingOrphans.push_back({ it->second.buffer, it->second.memory });
                it->second.buffer = VK_NULL_HANDLE;
                it->second.memory = VK_NULL_HANDLE;
                it->second.size = 0;
                it->second.usage = 0;
            }

            void VulkanRenderDevice::ClampPendingDrawViewportsToFramebuffer(GpuId framebufferId,
                                                                            const FramebufferResource& fb)
            {
                if (framebufferId == 0 || fb.width <= 0 || fb.height <= 0 || pendingDraws.empty()) {
                    return;
                }
                int clamped = 0;
                for (DrawCommand& draw : pendingDraws) {
                    if (draw.targetFramebuffer != framebufferId) continue;
                    if (draw.viewport[2] > fb.width) {
                        draw.viewport[2] = fb.width;
                        ++clamped;
                    }
                    if (draw.viewport[3] > fb.height) {
                        draw.viewport[3] = fb.height;
                        ++clamped;
                    }
                }
                }

            void VulkanRenderDevice::UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size)
            {
                                OrphanUniformBufferIfDeferred(buffer);
                OrphanBufferIfInFlight(buffer);
                SetUniformBufferData(buffer, data, size, BufferUsage::Dynamic);
            }

            void VulkanRenderDevice::BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint)
            {
                if (bindingPoint < 3) {
                    boundUBO[bindingPoint] = buffer;
                } else if (bindingPoint == kDDGIUBOBinding) {
                    boundDDGIUBO = buffer;
                }
            }

            void VulkanRenderDevice::BindArrayBuffer(GpuId buffer) { currentArrayBuffer = buffer; }
            void VulkanRenderDevice::BindElementBuffer(GpuId buffer)
            {
                currentElementBuffer = buffer;
                if (currentVAO != kInvalidGpuId) {
                    auto it = vaos.find(currentVAO);
                    if (it != vaos.end()) {
                        it->second.elementBuffer = buffer;
                        ++it->second.generation;
                    }
                }
            }

            // ---------------------------------------------------------------------------
            // Textures
            // ---------------------------------------------------------------------------

            VkFormat VulkanRenderDevice::ToVkFormat(TextureFormat format, bool& isDepth)
            {
                isDepth = false;
                switch (format) {
                case TextureFormat::R8: return VK_FORMAT_R8_UNORM;
                case TextureFormat::RGB8: return VK_FORMAT_R8G8B8_UNORM;
                case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
                case TextureFormat::SRGB8: return VK_FORMAT_R8G8B8_SRGB;
                case TextureFormat::SRGBA8: return VK_FORMAT_R8G8B8A8_SRGB;
                case TextureFormat::Depth24: isDepth = true; return VK_FORMAT_D32_SFLOAT;
                case TextureFormat::Depth32F: isDepth = true; return VK_FORMAT_D32_SFLOAT;
                default: return VK_FORMAT_R8G8B8A8_UNORM;
                }
            }

            VkFilter VulkanRenderDevice::ToVkFilter(TextureFilter filter)
            {
                switch (filter) {
                case TextureFilter::Nearest: return VK_FILTER_NEAREST;
                default: return VK_FILTER_LINEAR;
                }
            }

            VkSamplerAddressMode VulkanRenderDevice::ToVkWrap(TextureWrap wrap)
            {
                switch (wrap) {
                case TextureWrap::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case TextureWrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                case TextureWrap::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                }
            }

            VkPrimitiveTopology VulkanRenderDevice::ToVkTopology(PrimitiveTopology topology)
            {
                switch (topology) {
                case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                case PrimitiveTopology::Lines: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
                case PrimitiveTopology::Points: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
                default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                }
            }

            VkCompareOp VulkanRenderDevice::ToVkCompareOp(DepthFunc func)
            {
                switch (func) {
                case DepthFunc::LEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
                case DepthFunc::Greater: return VK_COMPARE_OP_GREATER;
                case DepthFunc::GEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
                case DepthFunc::Always: return VK_COMPARE_OP_ALWAYS;
                case DepthFunc::Never: return VK_COMPARE_OP_NEVER;
                case DepthFunc::Equal: return VK_COMPARE_OP_EQUAL;
                case DepthFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
                default: return VK_COMPARE_OP_LESS;
                }
            }

            VkBlendFactor VulkanRenderDevice::ToVkBlendFactor(int glBlendFactor)
            {
                switch (glBlendFactor) {
                case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
                case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
                case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
                case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
                case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
                case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
                case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                default: return VK_BLEND_FACTOR_ONE;
                }
            }

            GpuId VulkanRenderDevice::CreateTexture2D()
            {
                const GpuId id = nextId++;
                textures[id] = {};
                return id;
            }

            void VulkanRenderDevice::DestroyTexture(GpuId texture)
            {
                RemoveImGuiTexture(texture);

                for (auto& [fbId, fb] : framebuffers) {
                    bool touched = false;
                    if (fb.colorTexture == texture) { fb.colorTexture = kInvalidGpuId; touched = true; }
                    if (fb.depthTexture == texture) { fb.depthTexture = kInvalidGpuId; touched = true; }
                    if (touched) {
                        DestroyFramebufferGpu(fbId, fb);
                    }
                }

                auto it = textures.find(texture);
                if (it == textures.end()) return;
                OrphanTextureGpuResources(it->second);
                textures.erase(it);
            }

            void VulkanRenderDevice::RecreateSampler(TextureResource& res)
            {
                if (res.sampler) {
                    pendingTextureOrphans.push_back(
                        { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, res.sampler });
                    res.sampler = VK_NULL_HANDLE;
                }
                res.sampler = CreateSamplerRaw(
                    ToVkFilter(res.minFilter), ToVkFilter(res.magFilter),
                    ToVkWrap(res.wrapS), res.isDepth && !res.depthCompareDisabled);
            }

            void VulkanRenderDevice::SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                                     const void* pixels, bool /*generateMipmaps*/)
            {
                auto it = textures.find(texture);
                if (it == textures.end() || width <= 0 || height <= 0) return;
                TextureResource& res = it->second;

                bool isDepth = false;
                VkFormat vkFormat = ToVkFormat(format, isDepth);
                if (format == TextureFormat::RGB8 || format == TextureFormat::SRGB8) {
                    vkFormat = (format == TextureFormat::SRGB8) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                }

                // Drop any ImGui descriptor that still references the old view/sampler.
                RemoveImGuiTexture(texture);
                // Never destroy GPU objects immediately â€” prior frames' descriptor sets may
                // still reference them (font atlas re-uploads are the common trigger).
                OrphanTextureGpuResources(res);

                int channels = 4;
                if (format == TextureFormat::R8) channels = 1;
                else if (format == TextureFormat::RGB8 || format == TextureFormat::SRGB8) channels = 4;
                else if (format == TextureFormat::RGBA8 || format == TextureFormat::SRGBA8) channels = 4;

                res.width = width;
                res.height = height;
                res.format = vkFormat;
                res.isDepth = isDepth;
                res.isCubemap = false;
                res.layerCount = 1;

                VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                if (isDepth) usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                else usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                if (!CreateImageRaw(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                        vkFormat, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        1, 0, res.image, res.memory)) {
                    RTB_ERROR("VulkanRenderDevice: SetTexture2DData CreateImageRaw failed");
                    return;
                }

                const VkImageAspectFlags aspect = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                res.view = CreateImageViewRaw(res.image, vkFormat, aspect, VK_IMAGE_VIEW_TYPE_2D, 1);

                if (pixels && !isDepth) {
                    const std::size_t srcChannels = (format == TextureFormat::R8) ? 1
                        : (format == TextureFormat::RGB8 || format == TextureFormat::SRGB8) ? 3 : 4;
                    const std::size_t dstSize = static_cast<std::size_t>(width) * height * channels;
                    std::vector<std::uint8_t> rgba(dstSize, 255);
                    const auto* src = static_cast<const std::uint8_t*>(pixels);
                    if (srcChannels == static_cast<std::size_t>(channels)) {
                        std::memcpy(rgba.data(), src, dstSize);
                    }
                    else if (srcChannels == 3 && channels == 4) {
                        for (int i = 0, px = width * height; i < px; ++i) {
                            rgba[i * 4 + 0] = src[i * 3 + 0];
                            rgba[i * 4 + 1] = src[i * 3 + 1];
                            rgba[i * 4 + 2] = src[i * 3 + 2];
                            rgba[i * 4 + 3] = 255;
                        }
                    }
                    else if (srcChannels == 1 && channels == 4) {
                        for (int i = 0, px = width * height; i < px; ++i) {
                            rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = src[i];
                            rgba[i * 4 + 3] = 255;
                        }
                    }

                    VkBuffer staging = VK_NULL_HANDLE;
                    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                    if (CreateBufferRaw(dstSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            staging, stagingMem)) {
                        UploadHostVisibleBuffer(stagingMem, rgba.data(), dstSize);
                        TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspect, 1);
                        CopyBufferToImage(staging, res.image, static_cast<std::uint32_t>(width),
                            static_cast<std::uint32_t>(height), 0);
                        TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aspect, 1);
                        vkDestroyBuffer(device, staging, nullptr);
                        vkFreeMemory(device, stagingMem, nullptr);
                    }
                }
                else {
                    TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_UNDEFINED,
                        isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        aspect, 1);
                }

                RecreateSampler(res);
            }

            void VulkanRenderDevice::SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter)
            {
                auto it = textures.find(texture);
                if (it == textures.end()) return;
                it->second.minFilter = minFilter;
                it->second.magFilter = magFilter;
                if (it->second.image) {
                    RemoveImGuiTexture(texture);
                    RecreateSampler(it->second);
                }
            }

            void VulkanRenderDevice::SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT)
            {
                auto it = textures.find(texture);
                if (it == textures.end()) return;
                it->second.wrapS = wrapS;
                it->second.wrapT = wrapT;
                if (it->second.image) {
                    RemoveImGuiTexture(texture);
                    RecreateSampler(it->second);
                }
            }

            void VulkanRenderDevice::SetTexture2DDepthShadowParams(GpuId texture)
            {
                auto it = textures.find(texture);
                if (it == textures.end()) return;
                // Matches OpenGL: Nearest filters, ClampToBorder wrap (white border) and no
                // hardware depth compare -- the shadow comparison is done manually in basic.frag.
                it->second.minFilter = TextureFilter::Nearest;
                it->second.magFilter = TextureFilter::Nearest;
                it->second.wrapS = TextureWrap::ClampToBorder;
                it->second.wrapT = TextureWrap::ClampToBorder;
                it->second.isDepth = true;
                it->second.depthCompareDisabled = true;
                if (it->second.image) {
                    RemoveImGuiTexture(texture);
                    RecreateSampler(it->second);
                }
            }

            void VulkanRenderDevice::BindTexture2D(GpuId texture, unsigned int slot)
            {
                if (slot == kDDGIIrradianceBinding) {
                    boundDDGIIrradiance = texture;
                } else if (slot == kDDGIDistanceBinding) {
                    boundDDGIDistance = texture;
                } else {
                    boundTextureSlots[slot] = texture;
                }
            }

            void VulkanRenderDevice::UnbindTexture2D() {}

            GpuId VulkanRenderDevice::CreateCubemap()
            {
                const GpuId id = nextId++;
                TextureResource res{};
                res.isCubemap = true;
                res.layerCount = 6;
                textures[id] = res;
                return id;
            }

            void VulkanRenderDevice::SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                                   int width, int height, const void* pixels)
            {
                auto it = textures.find(cubemap);
                if (it == textures.end() || faceIndex < 0 || faceIndex > 5 || !pixels) return;
                TextureResource& res = it->second;

                if (!res.image) {
                    bool isDepth = false;
                    VkFormat vkFormat = ToVkFormat(format, isDepth);
                    if (format == TextureFormat::RGB8 || format == TextureFormat::SRGB8) {
                        vkFormat = (format == TextureFormat::SRGB8) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                    }
                    res.width = width;
                    res.height = height;
                    res.format = vkFormat;
                    res.isCubemap = true;
                    res.layerCount = 6;
                    if (!CreateImageRaw(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                            vkFormat, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                            res.image, res.memory)) {
                        return;
                    }
                    res.view = CreateImageViewRaw(res.image, vkFormat, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_VIEW_TYPE_CUBE, 6);
                    TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6);
                }

                const std::size_t srcChannels = (format == TextureFormat::R8) ? 1
                    : (format == TextureFormat::RGB8 || format == TextureFormat::SRGB8) ? 3 : 4;
                const std::size_t dstSize = static_cast<std::size_t>(width) * height * 4;
                std::vector<std::uint8_t> rgba(dstSize, 255);
                const auto* src = static_cast<const std::uint8_t*>(pixels);
                if (srcChannels == 4) std::memcpy(rgba.data(), src, dstSize);
                else if (srcChannels == 3) {
                    for (int i = 0, px = width * height; i < px; ++i) {
                        rgba[i * 4 + 0] = src[i * 3 + 0];
                        rgba[i * 4 + 1] = src[i * 3 + 1];
                        rgba[i * 4 + 2] = src[i * 3 + 2];
                    }
                }

                VkBuffer staging = VK_NULL_HANDLE;
                VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                if (CreateBufferRaw(dstSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        staging, stagingMem)) {
                    UploadHostVisibleBuffer(stagingMem, rgba.data(), dstSize);
                    CopyBufferToImage(staging, res.image, static_cast<std::uint32_t>(width),
                        static_cast<std::uint32_t>(height), static_cast<std::uint32_t>(faceIndex));
                    vkDestroyBuffer(device, staging, nullptr);
                    vkFreeMemory(device, stagingMem, nullptr);
                }

                if (faceIndex == 5) {
                    TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6);
                    RecreateSampler(res);
                }
            }

            void VulkanRenderDevice::SetCubemapFilterWrap(GpuId cubemap)
            {
                auto it = textures.find(cubemap);
                if (it == textures.end()) return;
                it->second.minFilter = TextureFilter::Linear;
                it->second.magFilter = TextureFilter::Linear;
                it->second.wrapS = TextureWrap::ClampToEdge;
                it->second.wrapT = TextureWrap::ClampToEdge;
                if (it->second.image) RecreateSampler(it->second);
            }

            void VulkanRenderDevice::BindCubemap(GpuId cubemap, unsigned int /*slot*/)
            {
                boundCubemapSlot0 = cubemap;
            }

            // ---------------------------------------------------------------------------
            // Framebuffers (real offscreen color+depth / depth-only targets, replayed as
            // ordered render-pass segments in Present())
            // ---------------------------------------------------------------------------

            GpuId VulkanRenderDevice::CreateFramebuffer()
            {
                const GpuId id = nextId++;
                framebuffers[id] = FramebufferResource{};
                return id;
            }

            void VulkanRenderDevice::DestroyFramebuffer(GpuId framebuffer)
            {
                auto it = framebuffers.find(framebuffer);
                if (it == framebuffers.end()) return;
                std::size_t references = 0;
                for (const DrawCommand& draw : pendingDraws) {
                    if (draw.targetFramebuffer == framebuffer) ++references;
                }
                if (references > 0) {
                                        deferredFramebufferDestroys.push_back(framebuffer);
                    if (currentBoundFramebuffer == framebuffer) {
                        currentBoundFramebuffer = 0;
                    }
                    return;
                }
                DestroyFramebufferGpu(framebuffer, it->second);
                framebuffers.erase(it);
                if (currentBoundFramebuffer == framebuffer) {
                    currentBoundFramebuffer = 0;
                }
            }

            void VulkanRenderDevice::BindFramebuffer(GpuId framebuffer)
            {
                if (framebuffer == kInvalidGpuId || framebuffers.find(framebuffer) == framebuffers.end()) {
                    currentBoundFramebuffer = 0;
                }
                else {
                    currentBoundFramebuffer = framebuffer;
                }
            }

            void VulkanRenderDevice::UnbindFramebuffer() { currentBoundFramebuffer = 0; }

            void VulkanRenderDevice::AttachFramebufferColorTexture(GpuId framebuffer, GpuId texture)
            {
                auto it = framebuffers.find(framebuffer);
                if (it == framebuffers.end()) return;
                it->second.colorTexture = texture;
                // Color without depth → continue/load pass used by volumetric fog.
                if (it->second.depthTexture == kInvalidGpuId) {
                    it->second.depthOnly = false;
                    it->second.colorOnlyLoad = true;
                }
                RebuildFramebufferGpu(framebuffer, it->second);
            }

            void VulkanRenderDevice::AttachFramebufferDepthTexture(GpuId framebuffer, GpuId texture)
            {
                auto it = framebuffers.find(framebuffer);
                if (it == framebuffers.end()) return;
                it->second.depthTexture = texture;
                it->second.colorOnlyLoad = false;
                RebuildFramebufferGpu(framebuffer, it->second);
            }

            void VulkanRenderDevice::SetFramebufferDrawReadNone()
            {
                if (currentBoundFramebuffer == 0) return;
                auto it = framebuffers.find(currentBoundFramebuffer);
                if (it == framebuffers.end()) return;
                it->second.depthOnly = true;
                RebuildFramebufferGpu(currentBoundFramebuffer, it->second);
            }

            bool VulkanRenderDevice::IsFramebufferComplete() const
            {
                if (currentBoundFramebuffer == 0) return true;
                auto it = framebuffers.find(currentBoundFramebuffer);
                if (it == framebuffers.end()) return false;
                return it->second.complete;
            }

            GpuId VulkanRenderDevice::CreateColorTextureForFramebuffer(int width, int height)
            {
                const GpuId id = CreateTexture2D();
                SetTexture2DData(id, TextureFormat::RGBA8, width, height, nullptr, false);
                SetTexture2DFilter(id, TextureFilter::Linear, TextureFilter::Linear);
                SetTexture2DWrap(id, TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
                return id;
            }

            GpuId VulkanRenderDevice::CreateDepthTextureForFramebuffer(int width, int height)
            {
                const GpuId id = CreateTexture2D();
                SetTexture2DData(id, TextureFormat::Depth32F, width, height, nullptr, false);
                // Shadow-sampling params (Nearest/ClampToBorder/no compare) are only applied
                // when the caller explicitly requests them via SetTexture2DDepthShadowParams.
                SetTexture2DFilter(id, TextureFilter::Nearest, TextureFilter::Nearest);
                return id;
            }

            void VulkanRenderDevice::InvalidatePipelinesForFramebuffer(GpuId framebufferId)
            {
                for (auto it = pipelineCache.begin(); it != pipelineCache.end(); ) {
                    if (it->first.targetFramebuffer == framebufferId) {
                        if (it->second) vkDestroyPipeline(device, it->second, nullptr);
                        it = pipelineCache.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }

            void VulkanRenderDevice::DestroyFramebufferGpu(GpuId framebufferId, FramebufferResource& fb)
            {
                if (!fb.framebuffer && !fb.renderPass) {
                    fb.complete = false;
                    return;
                }
                // Offscreen targets can be rebuilt (editor viewport resize) while a prior
                // frame's command buffer still references them â€” drain the GPU first.
                if (device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(device);
                }
                // Pipelines bind a specific VkRenderPass; drop any that targeted this FBO
                // before destroying the pass, otherwise Present() reuses a dead pipeline.
                InvalidatePipelinesForFramebuffer(framebufferId);
                if (fb.framebuffer) {
                    vkDestroyFramebuffer(device, fb.framebuffer, nullptr);
                    fb.framebuffer = VK_NULL_HANDLE;
                }
                if (fb.renderPass) {
                    vkDestroyRenderPass(device, fb.renderPass, nullptr);
                    fb.renderPass = VK_NULL_HANDLE;
                }
                fb.complete = false;
            }

            bool VulkanRenderDevice::RebuildFramebufferGpu(GpuId framebufferId, FramebufferResource& fb)
            {
                DestroyFramebufferGpu(framebufferId, fb);

                if (fb.depthOnly) {
                    auto dIt = textures.find(fb.depthTexture);
                    if (dIt == textures.end() || !dIt->second.image || !dIt->second.view) {
                        return false;
                    }
                    const TextureResource& depthTex = dIt->second;
                    fb.width = depthTex.width;
                    fb.height = depthTex.height;

                    if (!CreateOffscreenDepthOnlyRenderPass(depthTex.format, fb.renderPass)) {
                        return false;
                    }

                    VkFramebufferCreateInfo fi{};
                    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    fi.renderPass = fb.renderPass;
                    fi.attachmentCount = 1;
                    fi.pAttachments = &depthTex.view;
                    fi.width = static_cast<std::uint32_t>(fb.width);
                    fi.height = static_cast<std::uint32_t>(fb.height);
                    fi.layers = 1;
                    if (vkCreateFramebuffer(device, &fi, nullptr, &fb.framebuffer) != VK_SUCCESS) {
                        DestroyFramebufferGpu(framebufferId, fb);
                        return false;
                    }
                    fb.complete = true;
                                        ClampPendingDrawViewportsToFramebuffer(framebufferId, fb);
                    return true;
                }

                // Color-only LOAD pass (volumetric fog): share color, sample previous depth.
                if (fb.colorOnlyLoad || (fb.colorTexture != kInvalidGpuId && fb.depthTexture == kInvalidGpuId)) {
                    fb.colorOnlyLoad = true;
                    auto cIt = textures.find(fb.colorTexture);
                    if (cIt == textures.end() || !cIt->second.image || !cIt->second.view) {
                        return false;
                    }
                    const TextureResource& colorTex = cIt->second;
                    fb.width = colorTex.width;
                    fb.height = colorTex.height;

                    if (!CreateOffscreenColorOnlyLoadRenderPass(colorTex.format, fb.renderPass)) {
                        return false;
                    }

                    VkFramebufferCreateInfo fi{};
                    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    fi.renderPass = fb.renderPass;
                    fi.attachmentCount = 1;
                    fi.pAttachments = &colorTex.view;
                    fi.width = static_cast<std::uint32_t>(fb.width);
                    fi.height = static_cast<std::uint32_t>(fb.height);
                    fi.layers = 1;
                    if (vkCreateFramebuffer(device, &fi, nullptr, &fb.framebuffer) != VK_SUCCESS) {
                        DestroyFramebufferGpu(framebufferId, fb);
                        return false;
                    }
                    fb.complete = true;
                    ClampPendingDrawViewportsToFramebuffer(framebufferId, fb);
                    return true;
                }

                auto cIt = textures.find(fb.colorTexture);
                auto dIt = textures.find(fb.depthTexture);
                if (cIt == textures.end() || !cIt->second.image || !cIt->second.view
                    || dIt == textures.end() || !dIt->second.image || !dIt->second.view) {
                    return false;
                }
                const TextureResource& colorTex = cIt->second;
                const TextureResource& depthTex = dIt->second;
                fb.width = colorTex.width;
                fb.height = colorTex.height;

                if (!CreateOffscreenColorDepthRenderPass(colorTex.format, depthTex.format, fb.renderPass)) {
                    return false;
                }

                const VkImageView attachments[] = { colorTex.view, depthTex.view };
                VkFramebufferCreateInfo fi{};
                fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fi.renderPass = fb.renderPass;
                fi.attachmentCount = 2;
                fi.pAttachments = attachments;
                fi.width = static_cast<std::uint32_t>(fb.width);
                fi.height = static_cast<std::uint32_t>(fb.height);
                fi.layers = 1;
                if (vkCreateFramebuffer(device, &fi, nullptr, &fb.framebuffer) != VK_SUCCESS) {
                    DestroyFramebufferGpu(framebufferId, fb);
                    return false;
                }
                fb.complete = true;
                                ClampPendingDrawViewportsToFramebuffer(framebufferId, fb);
                return true;
            }

            bool VulkanRenderDevice::CreateOffscreenColorDepthRenderPass(VkFormat colorFmt, VkFormat depthFmt, VkRenderPass& outPass) const
            {
                VkAttachmentDescription color{};
                color.format = colorFmt;
                color.samples = VK_SAMPLE_COUNT_1_BIT;
                color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                VkAttachmentDescription depth{};
                depth.format = depthFmt;
                depth.samples = VK_SAMPLE_COUNT_1_BIT;
                depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

                VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
                VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorRef;
                subpass.pDepthStencilAttachment = &depthRef;

                VkSubpassDependency depIn{};
                depIn.srcSubpass = VK_SUBPASS_EXTERNAL;
                depIn.dstSubpass = 0;
                depIn.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depIn.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                depIn.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                depIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                VkSubpassDependency depOut{};
                depOut.srcSubpass = 0;
                depOut.dstSubpass = VK_SUBPASS_EXTERNAL;
                depOut.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                depOut.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                depOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                const VkAttachmentDescription attachments[] = { color, depth };
                const VkSubpassDependency deps[] = { depIn, depOut };
                VkRenderPassCreateInfo rp{};
                rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                rp.attachmentCount = 2;
                rp.pAttachments = attachments;
                rp.subpassCount = 1;
                rp.pSubpasses = &subpass;
                rp.dependencyCount = 2;
                rp.pDependencies = deps;
                return vkCreateRenderPass(device, &rp, nullptr, &outPass) == VK_SUCCESS;
            }

            bool VulkanRenderDevice::CreateOffscreenDepthOnlyRenderPass(VkFormat depthFmt, VkRenderPass& outPass) const
            {
                VkAttachmentDescription depth{};
                depth.format = depthFmt;
                depth.samples = VK_SAMPLE_COUNT_1_BIT;
                depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

                VkAttachmentReference depthRef{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 0;
                subpass.pColorAttachments = nullptr;
                subpass.pDepthStencilAttachment = &depthRef;

                VkSubpassDependency depIn{};
                depIn.srcSubpass = VK_SUBPASS_EXTERNAL;
                depIn.dstSubpass = 0;
                depIn.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depIn.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                depIn.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                depIn.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                VkSubpassDependency depOut{};
                depOut.srcSubpass = 0;
                depOut.dstSubpass = VK_SUBPASS_EXTERNAL;
                depOut.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                depOut.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depOut.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                depOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                const VkSubpassDependency deps[] = { depIn, depOut };
                VkRenderPassCreateInfo rp{};
                rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                rp.attachmentCount = 1;
                rp.pAttachments = &depth;
                rp.subpassCount = 1;
                rp.pSubpasses = &subpass;
                rp.dependencyCount = 2;
                rp.pDependencies = deps;
                return vkCreateRenderPass(device, &rp, nullptr, &outPass) == VK_SUCCESS;
            }

            bool VulkanRenderDevice::CreateOffscreenColorOnlyLoadRenderPass(VkFormat colorFmt, VkRenderPass& outPass) const
            {
                // Continues an existing offscreen color target after the depth+color pass ended
                // (color finalLayout = SHADER_READ_ONLY). Used by volumetric fog so scene depth
                // can be sampled while writing color.
                VkAttachmentDescription color{};
                color.format = colorFmt;
                color.samples = VK_SAMPLE_COUNT_1_BIT;
                color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorRef;
                subpass.pDepthStencilAttachment = nullptr;

                VkSubpassDependency depIn{};
                depIn.srcSubpass = VK_SUBPASS_EXTERNAL;
                depIn.dstSubpass = 0;
                depIn.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depIn.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                depIn.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                depIn.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                VkSubpassDependency depOut{};
                depOut.srcSubpass = 0;
                depOut.dstSubpass = VK_SUBPASS_EXTERNAL;
                depOut.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                depOut.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                depOut.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                depOut.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                const VkSubpassDependency deps[] = { depIn, depOut };
                VkRenderPassCreateInfo rp{};
                rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                rp.attachmentCount = 1;
                rp.pAttachments = &color;
                rp.subpassCount = 1;
                rp.pSubpasses = &subpass;
                rp.dependencyCount = 2;
                rp.pDependencies = deps;
                return vkCreateRenderPass(device, &rp, nullptr, &outPass) == VK_SUCCESS;
            }

            // ---------------------------------------------------------------------------
            // VAO / draws
            // ---------------------------------------------------------------------------

            GpuId VulkanRenderDevice::CreateVertexArray()
            {
                const GpuId id = nextId++;
                vaos[id] = {};
                return id;
            }

            void VulkanRenderDevice::DestroyVertexArray(GpuId vao)
            {
                std::size_t references = 0;
                for (const DrawCommand& draw : pendingDraws) {
                    if (draw.vao == vao) ++references;
                }
                if (references > 0) {
                                        deferredVaoDestroys.push_back(vao);
                    if (currentVAO == vao) currentVAO = kInvalidGpuId;
                    return;
                }
                vaos.erase(vao);
                if (currentVAO == vao) currentVAO = kInvalidGpuId;
            }

            void VulkanRenderDevice::BindVertexArray(GpuId vao) { currentVAO = vao; }
            void VulkanRenderDevice::UnbindVertexArray() { currentVAO = kInvalidGpuId; }

            void VulkanRenderDevice::EnableVertexAttribFloat(unsigned int location, int components, int stride, std::size_t offset)
            {
                if (currentVAO == kInvalidGpuId) return;
                auto it = vaos.find(currentVAO);
                if (it == vaos.end()) return;
                // Preserve divisor: Draw* paths often re-bind attribs each frame; resetting
                // divisor to 0 breaks instancing (particle quads become streaks/lines).
                unsigned int prevDivisor = 0;
                const auto existing = it->second.attributes.find(location);
                if (existing != it->second.attributes.end()) {
                    prevDivisor = existing->second.divisor;
                }
                VertexAttribBinding binding{};
                binding.buffer = currentArrayBuffer;
                binding.components = components;
                binding.stride = stride;
                binding.offset = offset;
                binding.isInt = false;
                binding.divisor = prevDivisor;
                it->second.attributes[location] = binding;
                ++it->second.generation;
            }

            void VulkanRenderDevice::EnableVertexAttribInt(unsigned int location, int components, int stride, std::size_t offset)
            {
                if (currentVAO == kInvalidGpuId) return;
                auto it = vaos.find(currentVAO);
                if (it == vaos.end()) return;
                unsigned int prevDivisor = 0;
                const auto existing = it->second.attributes.find(location);
                if (existing != it->second.attributes.end()) {
                    prevDivisor = existing->second.divisor;
                }
                VertexAttribBinding binding{};
                binding.buffer = currentArrayBuffer;
                binding.components = components;
                binding.stride = stride;
                binding.offset = offset;
                binding.isInt = true;
                binding.divisor = prevDivisor;
                it->second.attributes[location] = binding;
                ++it->second.generation;
            }

            void VulkanRenderDevice::SetVertexAttribDivisor(unsigned int location, unsigned int divisor)
            {
                if (currentVAO == kInvalidGpuId) return;
                auto it = vaos.find(currentVAO);
                if (it == vaos.end()) return;
                auto ait = it->second.attributes.find(location);
                if (ait == it->second.attributes.end()) return;
                ait->second.divisor = divisor;
                ++it->second.generation;
            }

            void VulkanRenderDevice::RecordDrawCommand(PrimitiveTopology topology, bool indexed, IndexType indexType,
                                                      int count, int first, int instanceCount)
            {
                if (!initialized) return;
                if (currentProgram == kInvalidGpuId || currentVAO == kInvalidGpuId || count <= 0) return;
                if (pendingDraws.size() >= kMaxDrawsPerFrame) return;

                auto vaoIt = vaos.find(currentVAO);
                if (vaoIt == vaos.end()) return;

                DrawCommand cmd{};
                cmd.program = currentProgram;
                cmd.vao = currentVAO;
                cmd.vaoGeneration = vaoIt->second.generation;
                cmd.topology = topology;
                cmd.indexed = indexed;
                cmd.indexType = indexType;
                cmd.count = count;
                cmd.first = first;
                cmd.instanceCount = std::max(1, instanceCount);
                cmd.depthTest = stateDepthTest;
                cmd.depthWrite = stateDepthWrite;
                cmd.depthFunc = stateDepthFunc;
                cmd.cullFace = stateCullFace;
                cmd.blend = stateBlend;
                cmd.srcRGB = stateSrcRGB;
                cmd.dstRGB = stateDstRGB;
                cmd.srcAlpha = stateSrcAlpha;
                cmd.dstAlpha = stateDstAlpha;
                for (int i = 0; i < 4; ++i) cmd.colorMask[i] = stateColorMask[i];
                for (int i = 0; i < 4; ++i) cmd.viewport[i] = pendingViewport[i];
                cmd.uboLighting = boundUBO[0];
                cmd.uboCamera = boundUBO[1];
                cmd.uboBone = boundUBO[2];
                cmd.vkLighting = ResolveBufferHandle(boundUBO[0]);
                cmd.vkCamera = ResolveBufferHandle(boundUBO[1]);
                cmd.vkBone = ResolveBufferHandle(boundUBO[2]);
                {
                    const std::vector<GpuId> bufferOrder = ComputeVaoBufferOrder(vaoIt->second);
                    cmd.vkVertexBuffers.reserve(bufferOrder.size());
                    for (GpuId bid : bufferOrder) {
                        cmd.vkVertexBuffers.push_back(ResolveBufferHandle(bid));
                    }
                    if (indexed) {
                        cmd.vkIndexBuffer = ResolveBufferHandle(vaoIt->second.elementBuffer);
                    }
                }
                auto t0 = boundTextureSlots.find(0);
                auto t1 = boundTextureSlots.find(1);
                cmd.texSlot0 = (t0 != boundTextureSlots.end()) ? t0->second : kInvalidGpuId;
                cmd.texSlot1 = (t1 != boundTextureSlots.end()) ? t1->second : kInvalidGpuId;
                cmd.cubemapSlot0 = boundCubemapSlot0;
                cmd.ddgiUBO = boundDDGIUBO;
                cmd.ddgiIrradiance = boundDDGIIrradiance;
                cmd.ddgiDistance = boundDDGIDistance;
                cmd.perDraw = currentPerDraw;
                cmd.targetFramebuffer = currentBoundFramebuffer;
                if (currentBoundFramebuffer != 0) {
                    auto fbIt = framebuffers.find(currentBoundFramebuffer);
                    cmd.depthOnly = (fbIt != framebuffers.end()) && fbIt->second.depthOnly;
                }
                                pendingDraws.push_back(cmd);
            }

            void VulkanRenderDevice::DrawIndexed(PrimitiveTopology topology, int indexCount, IndexType indexType)
            {
                RecordDrawCommand(topology, true, indexType, indexCount, 0, 1);
            }

            void VulkanRenderDevice::DrawIndexedInstanced(PrimitiveTopology topology, int indexCount, IndexType indexType, int instanceCount)
            {
                RecordDrawCommand(topology, true, indexType, indexCount, 0, instanceCount);
            }

            void VulkanRenderDevice::DrawArrays(PrimitiveTopology topology, int first, int count)
            {
                RecordDrawCommand(topology, false, IndexType::UInt32, count, first, 1);
            }

            void VulkanRenderDevice::DrawArraysInstanced(PrimitiveTopology topology, int first, int count, int instanceCount)
            {
                RecordDrawCommand(topology, false, IndexType::UInt32, count, first, instanceCount);
            }

            std::uintptr_t VulkanRenderDevice::GetNativeTextureIdForImGui(GpuId texture) const
            {
                if (!imguiBackendInitialized || texture == kInvalidGpuId) return 0;

                auto cached = imguiTextureSets.find(texture);
                if (cached != imguiTextureSets.end()) {
                    return reinterpret_cast<std::uintptr_t>(cached->second);
                }

                auto it = textures.find(texture);
                if (it == textures.end() || !it->second.view || !it->second.sampler) {
                    return 0;
                }

                const VkImageLayout layout = it->second.isDepth
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(it->second.sampler, it->second.view, layout);
                imguiTextureSets[texture] = set;
                return reinterpret_cast<std::uintptr_t>(set);
            }

            void VulkanRenderDevice::RemoveImGuiTexture(GpuId texture)
            {
                auto it = imguiTextureSets.find(texture);
                if (it == imguiTextureSets.end()) return;
                if (it->second && imguiBackendInitialized) {
                    // Descriptor sets may still be referenced by in-flight ImGui draws.
                    if (device != VK_NULL_HANDLE) {
                        vkDeviceWaitIdle(device);
                    }
                    ImGui_ImplVulkan_RemoveTexture(it->second);
                }
                imguiTextureSets.erase(it);
            }

            // ---------------------------------------------------------------------------
            // ImGui backend lifecycle
            // ---------------------------------------------------------------------------

            bool VulkanRenderDevice::InitializeImGuiBackend(SDL_Window* sdlWindow)
            {
                if (imguiBackendInitialized) return true;
                if (!ImGui_ImplSDL2_InitForVulkan(sdlWindow)) return false;

                ImGui_ImplVulkan_InitInfo info{};
                info.ApiVersion = vulkanApiVersion;
                info.Instance = instance;
                info.PhysicalDevice = physicalDevice;
                info.Device = device;
                info.QueueFamily = graphicsQueueFamily;
                info.Queue = graphicsQueue;
                info.DescriptorPoolSize = 1000; // backend creates its own pool with the FREE bit
                info.MinImageCount = 2;
                info.ImageCount = GetSwapchainImageCount();
                info.PipelineInfoMain.RenderPass = renderPass;
                info.PipelineInfoMain.Subpass = 0;
                info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
                if (!ImGui_ImplVulkan_Init(&info)) {
                    ImGui_ImplSDL2_Shutdown();
                    return false;
                }

                imguiBackendInitialized = true;
                return true;
            }

            void VulkanRenderDevice::ShutdownImGuiBackend()
            {
                if (!imguiBackendInitialized) return;
                vkDeviceWaitIdle(device);
                for (auto& [id, set] : imguiTextureSets) {
                    (void)id;
                    if (set) ImGui_ImplVulkan_RemoveTexture(set);
                }
                imguiTextureSets.clear();
                ImGui_ImplVulkan_Shutdown();
                ImGui_ImplSDL2_Shutdown();
                imguiBackendInitialized = false;
                pendingImGuiDrawData = nullptr;
            }

            void VulkanRenderDevice::BeginImGuiFrame()
            {
                if (!imguiBackendInitialized) return;
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL2_NewFrame();
            }

            void VulkanRenderDevice::QueueImGuiDrawData(ImDrawData* drawData)
            {
                pendingImGuiDrawData = drawData;
            }

            std::vector<GpuId> VulkanRenderDevice::ComputeVaoBufferOrder(const VaoResource& vao)
            {
                std::vector<GpuId> order;
                std::vector<unsigned int> locations;
                locations.reserve(vao.attributes.size());
                for (const auto& [loc, _] : vao.attributes) locations.push_back(loc);
                std::sort(locations.begin(), locations.end());
                for (unsigned int loc : locations) {
                    const GpuId bid = vao.attributes.at(loc).buffer;
                    if (std::find(order.begin(), order.end(), bid) == order.end()) {
                        order.push_back(bid);
                    }
                }
                return order;
            }

            // ---------------------------------------------------------------------------
            // Pipeline cache
            // ---------------------------------------------------------------------------

            VkPipeline VulkanRenderDevice::GetOrCreatePipeline(const DrawCommand& cmd)
            {
                auto progIt = programs.find(cmd.program);
                auto vaoIt = vaos.find(cmd.vao);
                if (progIt == programs.end() || !progIt->second.valid || vaoIt == vaos.end()) {
                    return VK_NULL_HANDLE;
                }

                PipelineKey key{};
                key.program = cmd.program;
                key.vao = cmd.vao;
                // Vulkan records draws until Present(). A VAO can gain optional instance
                // attributes later in the same frame, so pipeline identity must use the
                // layout that will actually be replayed, not the earlier recorded version.
                key.vaoGeneration = vaoIt->second.generation;
                key.topology = static_cast<int>(cmd.topology);
                key.depthTest = cmd.depthTest;
                key.depthWrite = cmd.depthWrite;
                key.depthFunc = static_cast<int>(cmd.depthFunc);
                key.cullFace = cmd.cullFace;
                key.blend = cmd.blend;
                key.srcRGB = cmd.srcRGB; key.dstRGB = cmd.dstRGB;
                key.srcAlpha = cmd.srcAlpha; key.dstAlpha = cmd.dstAlpha;
                for (int i = 0; i < 4; ++i) key.colorMask[i] = cmd.colorMask[i];
                key.targetFramebuffer = cmd.targetFramebuffer;
                key.depthOnly = cmd.depthOnly;

                auto pit = pipelineCache.find(key);
                if (pit != pipelineCache.end()) {
                    return pit->second;
                }

                const VaoResource& vao = vaoIt->second;
                const ShaderProgramResource& prog = progIt->second;

                // Only declare attributes the vertex shader consumes (shadow.vert skips
                // normals/UVs that a full mesh VAO still exposes). Binding order must
                // stay identical to ReplayDraw's ComputeVaoBufferOrder.
                auto shaderUsesLoc = [&](unsigned int loc) -> bool {
                    if (prog.usedVertexLocations.empty()) return true;
                    return std::find(prog.usedVertexLocations.begin(), prog.usedVertexLocations.end(), loc)
                        != prog.usedVertexLocations.end();
                };

                const std::vector<GpuId> bufferOrder = ComputeVaoBufferOrder(vao);

                std::vector<VkVertexInputBindingDescription> bindings;
                bindings.reserve(bufferOrder.size() + 1);
                for (std::uint32_t bi = 0; bi < bufferOrder.size(); ++bi) {
                    int stride = 0;
                    unsigned int divisor = 0;
                    for (const auto& [loc, attr] : vao.attributes) {
                        if (attr.buffer == bufferOrder[bi]) {
                            stride = attr.stride;
                            divisor = attr.divisor;
                            break;
                        }
                    }
                    VkVertexInputBindingDescription b{};
                    b.binding = bi;
                    b.stride = static_cast<std::uint32_t>(stride);
                    b.inputRate = divisor > 0 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
                    bindings.push_back(b);
                }

                auto bindingIndexOf = [&](GpuId bid) -> std::uint32_t {
                    for (std::uint32_t i = 0; i < bufferOrder.size(); ++i) {
                        if (bufferOrder[i] == bid) return i;
                    }
                    return 0;
                };

                std::vector<VkVertexInputAttributeDescription> attrs;
                attrs.reserve(vao.attributes.size() + 10);
                for (const auto& [loc, attr] : vao.attributes) {
                    if (!shaderUsesLoc(loc)) continue;
                    VkVertexInputAttributeDescription a{};
                    a.location = loc;
                    a.binding = bindingIndexOf(attr.buffer);
                    a.offset = static_cast<std::uint32_t>(attr.offset);
                    if (attr.isInt) {
                        switch (attr.components) {
                        case 1: a.format = VK_FORMAT_R32_SINT; break;
                        case 2: a.format = VK_FORMAT_R32G32_SINT; break;
                        case 3: a.format = VK_FORMAT_R32G32B32_SINT; break;
                        default: a.format = VK_FORMAT_R32G32B32A32_SINT; break;
                        }
                    }
                    else {
                        switch (attr.components) {
                        case 1: a.format = VK_FORMAT_R32_SFLOAT; break;
                        case 2: a.format = VK_FORMAT_R32G32_SFLOAT; break;
                        case 3: a.format = VK_FORMAT_R32G32B32_SFLOAT; break;
                        default: a.format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
                        }
                    }
                    attrs.push_back(a);
                }

                // Zeroed fallbacks only for locations the shader needs but this VAO lacks.
                const std::uint32_t fallbackBinding = static_cast<std::uint32_t>(bindings.size());
                static const std::vector<unsigned int> kDefaultLocs = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
                const std::vector<unsigned int>& requiredLocs = prog.usedVertexLocations.empty()
                    ? kDefaultLocs
                    : prog.usedVertexLocations;
                bool needFallbackBinding = false;
                for (unsigned int loc : requiredLocs) {
                    if (vao.attributes.find(loc) == vao.attributes.end()) {
                        needFallbackBinding = true;
                        break;
                    }
                }
                if (needFallbackBinding) {
                    VkVertexInputBindingDescription fb{};
                    fb.binding = fallbackBinding;
                    fb.stride = 16;
                    fb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    bindings.push_back(fb);
                    for (unsigned int loc : requiredLocs) {
                        if (vao.attributes.find(loc) != vao.attributes.end()) continue;
                        VkVertexInputAttributeDescription a{};
                        a.location = loc;
                        a.binding = fallbackBinding;
                        a.offset = 0;
                        a.format = (loc == 3) ? VK_FORMAT_R32G32B32A32_SINT
                                              : VK_FORMAT_R32G32B32A32_SFLOAT;
                        attrs.push_back(a);
                    }
                }

                VkPipelineShaderStageCreateInfo stages[2]{};
                stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
                stages[0].module = progIt->second.vertModule;
                stages[0].pName = "main";
                stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                stages[1].module = progIt->second.fragModule;
                stages[1].pName = "main";

                VkPipelineVertexInputStateCreateInfo vi{};
                vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                vi.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
                vi.pVertexBindingDescriptions = bindings.data();
                vi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrs.size());
                vi.pVertexAttributeDescriptions = attrs.data();

                VkPipelineInputAssemblyStateCreateInfo ia{};
                ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                ia.topology = ToVkTopology(cmd.topology);

                VkPipelineViewportStateCreateInfo vp{};
                vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
                vp.viewportCount = 1;
                vp.scissorCount = 1;

                VkPipelineRasterizationStateCreateInfo rs{};
                rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                rs.polygonMode = VK_POLYGON_MODE_FILL;
                rs.cullMode = cmd.cullFace ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
                rs.frontFace = VK_FRONT_FACE_CLOCKWISE; // positive viewport + GL-style projection
                rs.lineWidth = 1.0f;
                // Depth bias on shadow pass reduces acne / streak artifacts on flat floors.
                if (cmd.depthOnly) {
                    rs.depthBiasEnable = VK_TRUE;
                    rs.depthBiasConstantFactor = 1.25f;
                    rs.depthBiasClamp = 0.0f;
                    rs.depthBiasSlopeFactor = 1.75f;
                }

                VkPipelineMultisampleStateCreateInfo ms{};
                ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

                VkPipelineDepthStencilStateCreateInfo ds{};
                ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                ds.depthTestEnable = cmd.depthTest ? VK_TRUE : VK_FALSE;
                ds.depthWriteEnable = cmd.depthWrite ? VK_TRUE : VK_FALSE;
                ds.depthCompareOp = ToVkCompareOp(cmd.depthFunc);

                VkPipelineColorBlendAttachmentState blendAtt{};
                blendAtt.colorWriteMask =
                    (cmd.colorMask[0] ? VK_COLOR_COMPONENT_R_BIT : 0)
                    | (cmd.colorMask[1] ? VK_COLOR_COMPONENT_G_BIT : 0)
                    | (cmd.colorMask[2] ? VK_COLOR_COMPONENT_B_BIT : 0)
                    | (cmd.colorMask[3] ? VK_COLOR_COMPONENT_A_BIT : 0);
                blendAtt.blendEnable = cmd.blend ? VK_TRUE : VK_FALSE;
                blendAtt.srcColorBlendFactor = ToVkBlendFactor(cmd.srcRGB);
                blendAtt.dstColorBlendFactor = ToVkBlendFactor(cmd.dstRGB);
                blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
                blendAtt.srcAlphaBlendFactor = ToVkBlendFactor(cmd.srcAlpha);
                blendAtt.dstAlphaBlendFactor = ToVkBlendFactor(cmd.dstAlpha);
                blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;

                VkPipelineColorBlendStateCreateInfo cb{};
                cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                if (cmd.depthOnly) {
                    cb.attachmentCount = 0;
                    cb.pAttachments = nullptr;
                }
                else {
                    cb.attachmentCount = 1;
                    cb.pAttachments = &blendAtt;
                }

                const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
                VkPipelineDynamicStateCreateInfo dyn{};
                dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
                dyn.dynamicStateCount = 2;
                dyn.pDynamicStates = dynStates;

                VkGraphicsPipelineCreateInfo pci{};
                pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pci.stageCount = 2;
                pci.pStages = stages;
                pci.pVertexInputState = &vi;
                pci.pInputAssemblyState = &ia;
                pci.pViewportState = &vp;
                pci.pRasterizationState = &rs;
                pci.pMultisampleState = &ms;
                pci.pDepthStencilState = &ds;
                pci.pColorBlendState = &cb;
                pci.pDynamicState = &dyn;
                pci.layout = pipelineLayout;
                pci.renderPass = ResolveRenderPassForTarget(cmd.targetFramebuffer);
                pci.subpass = 0;

                VkPipeline pipeline = VK_NULL_HANDLE;
                if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateGraphicsPipelines failed");
                    return VK_NULL_HANDLE;
                }
                pipelineCache.emplace(key, pipeline);
                return pipeline;
            }

            void VulkanRenderDevice::DestroyAllPipelines()
            {
                for (auto& [k, p] : pipelineCache) {
                    if (p) vkDestroyPipeline(device, p, nullptr);
                }
                pipelineCache.clear();
            }

            // ---------------------------------------------------------------------------
            // Resource helpers
            // ---------------------------------------------------------------------------

            std::uint32_t VulkanRenderDevice::FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags props) const
            {
                VkPhysicalDeviceMemoryProperties memProps{};
                vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
                for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                    if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
                        return i;
                    }
                }
                return 0;
            }

            bool VulkanRenderDevice::CreateBufferRaw(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                                    VkBuffer& outBuffer, VkDeviceMemory& outMemory) const
            {
                VkBufferCreateInfo bi{};
                bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bi.size = size;
                bi.usage = usage;
                bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                if (vkCreateBuffer(device, &bi, nullptr, &outBuffer) != VK_SUCCESS) return false;

                VkMemoryRequirements req{};
                vkGetBufferMemoryRequirements(device, outBuffer, &req);
                VkMemoryAllocateFlagsInfo flagsInfo{};
                flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
                if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
                    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
                }
                VkMemoryAllocateInfo ai{};
                ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                ai.pNext = (flagsInfo.flags != 0) ? &flagsInfo : nullptr;
                ai.allocationSize = req.size;
                ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
                if (vkAllocateMemory(device, &ai, nullptr, &outMemory) != VK_SUCCESS) {
                    vkDestroyBuffer(device, outBuffer, nullptr);
                    outBuffer = VK_NULL_HANDLE;
                    return false;
                }
                vkBindBufferMemory(device, outBuffer, outMemory, 0);
                return true;
            }

            VkCommandBuffer VulkanRenderDevice::BeginSingleTimeCommands() const
            {
                VkCommandBufferAllocateInfo ai{};
                ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                ai.commandPool = commandPool;
                ai.commandBufferCount = 1;
                VkCommandBuffer cmd = VK_NULL_HANDLE;
                vkAllocateCommandBuffers(device, &ai, &cmd);
                VkCommandBufferBeginInfo bi{};
                bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &bi);
                return cmd;
            }

            void VulkanRenderDevice::EndSingleTimeCommands(VkCommandBuffer cmd) const
            {
                vkEndCommandBuffer(cmd);
                VkSubmitInfo si{};
                si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                si.commandBufferCount = 1;
                si.pCommandBuffers = &cmd;
                vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
                vkQueueWaitIdle(graphicsQueue);
                vkFreeCommandBuffers(device, commandPool, 1, &cmd);
            }

            bool VulkanRenderDevice::CreateImageRaw(std::uint32_t width, std::uint32_t height, VkFormat format,
                                                   VkImageTiling tiling, VkImageUsageFlags usage,
                                                   VkMemoryPropertyFlags props, std::uint32_t arrayLayers,
                                                   VkImageCreateFlags flags, VkImage& outImage,
                                                   VkDeviceMemory& outMemory) const
            {
                VkImageCreateInfo ii{};
                ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                ii.flags = flags;
                ii.imageType = VK_IMAGE_TYPE_2D;
                ii.extent = { width, height, 1 };
                ii.mipLevels = 1;
                ii.arrayLayers = arrayLayers;
                ii.format = format;
                ii.tiling = tiling;
                ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                ii.usage = usage;
                ii.samples = VK_SAMPLE_COUNT_1_BIT;
                ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                if (vkCreateImage(device, &ii, nullptr, &outImage) != VK_SUCCESS) return false;

                VkMemoryRequirements req{};
                vkGetImageMemoryRequirements(device, outImage, &req);
                VkMemoryAllocateInfo ai{};
                ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                ai.allocationSize = req.size;
                ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
                if (vkAllocateMemory(device, &ai, nullptr, &outMemory) != VK_SUCCESS) {
                    vkDestroyImage(device, outImage, nullptr);
                    outImage = VK_NULL_HANDLE;
                    return false;
                }
                vkBindImageMemory(device, outImage, outMemory, 0);
                return true;
            }

            bool VulkanRenderDevice::CreateImage3DRaw(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                                                      VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                                                      VkMemoryPropertyFlags props, VkImage& outImage,
                                                      VkDeviceMemory& outMemory) const
            {
                VkImageCreateInfo ii{};
                ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                ii.imageType = VK_IMAGE_TYPE_3D;
                ii.extent = { width, height, depth };
                ii.mipLevels = 1;
                ii.arrayLayers = 1;
                ii.format = format;
                ii.tiling = tiling;
                ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                ii.usage = usage;
                ii.samples = VK_SAMPLE_COUNT_1_BIT;
                ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                if (vkCreateImage(device, &ii, nullptr, &outImage) != VK_SUCCESS) return false;

                VkMemoryRequirements req{};
                vkGetImageMemoryRequirements(device, outImage, &req);
                VkMemoryAllocateInfo ai{};
                ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                ai.allocationSize = req.size;
                ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
                if (vkAllocateMemory(device, &ai, nullptr, &outMemory) != VK_SUCCESS) {
                    vkDestroyImage(device, outImage, nullptr);
                    outImage = VK_NULL_HANDLE;
                    return false;
                }
                vkBindImageMemory(device, outImage, outMemory, 0);
                return true;
            }

            VkImageView VulkanRenderDevice::CreateImageViewRaw(VkImage image, VkFormat format, VkImageAspectFlags aspect,
                                                              VkImageViewType viewType, std::uint32_t layerCount) const
            {
                VkImageViewCreateInfo ci{};
                ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                ci.image = image;
                ci.viewType = viewType;
                ci.format = format;
                ci.subresourceRange.aspectMask = aspect;
                ci.subresourceRange.baseMipLevel = 0;
                ci.subresourceRange.levelCount = 1;
                ci.subresourceRange.baseArrayLayer = 0;
                ci.subresourceRange.layerCount = layerCount;
                VkImageView view = VK_NULL_HANDLE;
                vkCreateImageView(device, &ci, nullptr, &view);
                return view;
            }

            void VulkanRenderDevice::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                                          VkImageAspectFlags aspect, std::uint32_t layerCount) const
            {
                VkCommandBuffer cmd = BeginSingleTimeCommands();
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = oldLayout;
                barrier.newLayout = newLayout;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = aspect;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = layerCount;

                VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                    && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
                    && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                }
                else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
                    && (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                        || newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                }
                else {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = 0;
                }

                vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                EndSingleTimeCommands(cmd);
            }

            void VulkanRenderDevice::CopyBufferToImage(VkBuffer buffer, VkImage image, std::uint32_t width,
                                                      std::uint32_t height, std::uint32_t baseLayer) const
            {
                VkCommandBuffer cmd = BeginSingleTimeCommands();
                VkBufferImageCopy region{};
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = baseLayer;
                region.imageSubresource.layerCount = 1;
                region.imageExtent = { width, height, 1 };
                vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                EndSingleTimeCommands(cmd);
            }

            VkSampler VulkanRenderDevice::CreateSamplerRaw(VkFilter minFilter, VkFilter magFilter,
                                                          VkSamplerAddressMode addressMode, bool depthCompare) const
            {
                VkSamplerCreateInfo ci{};
                ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                ci.magFilter = magFilter;
                ci.minFilter = minFilter;
                ci.addressModeU = addressMode;
                ci.addressModeV = addressMode;
                ci.addressModeW = addressMode;
                ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
                if (depthCompare) {
                    ci.compareEnable = VK_TRUE;
                    ci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                }
                VkSampler sampler = VK_NULL_HANDLE;
                vkCreateSampler(device, &ci, nullptr, &sampler);
                return sampler;
            }

            // ---------------------------------------------------------------------------
            // Descriptor / PerDraw / fallbacks
            // ---------------------------------------------------------------------------

            bool VulkanRenderDevice::CreateDescriptorSetLayoutAndPipelineLayout()
            {
                std::array<VkDescriptorSetLayoutBinding, kDescriptorBindingCount> bindings{};
                for (std::uint32_t i = 0; i < 4; ++i) {
                    bindings[i].binding = i;
                    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    bindings[i].descriptorCount = 1;
                    bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                }
                for (std::uint32_t i = 4; i < 7; ++i) {
                    bindings[i].binding = i;
                    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    bindings[i].descriptorCount = 1;
                    bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
                }
                bindings[7].binding = 7;
                bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                bindings[7].descriptorCount = 1;
                bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                for (std::uint32_t i = 8; i < kDescriptorBindingCount; ++i) {
                    bindings[i].binding = i;
                    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    bindings[i].descriptorCount = 1;
                    bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                }

                VkDescriptorSetLayoutCreateInfo layoutInfo{};
                layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layoutInfo.bindingCount = kDescriptorBindingCount;
                layoutInfo.pBindings = bindings.data();
                if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateDescriptorSetLayout failed");
                    return false;
                }

                VkPipelineLayoutCreateInfo plInfo{};
                plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                plInfo.setLayoutCount = 1;
                plInfo.pSetLayouts = &descriptorSetLayout;
                if (vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreatePipelineLayout failed");
                    return false;
                }

                for (int f = 0; f < kMaxFramesInFlight; ++f) {
                    std::array<VkDescriptorPoolSize, 2> poolSizes{};
                    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    poolSizes[0].descriptorCount = kMaxDrawsPerFrame * 5;
                    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    poolSizes[1].descriptorCount = kMaxDrawsPerFrame * 5;

                    VkDescriptorPoolCreateInfo poolInfo{};
                    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
                    poolInfo.pPoolSizes = poolSizes.data();
                    poolInfo.maxSets = kMaxDrawsPerFrame;
                    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPools[f]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: vkCreateDescriptorPool failed");
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreatePerDrawBuffers()
            {
                const VkDeviceSize total = perDrawSlotStride * kMaxDrawsPerFrame;
                for (int f = 0; f < kMaxFramesInFlight; ++f) {
                    if (!CreateBufferRaw(total, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            perDrawBuffers[f], perDrawMemories[f])) {
                        return false;
                    }
                    if (vkMapMemory(device, perDrawMemories[f], 0, total, 0, &perDrawMapped[f]) != VK_SUCCESS) {
                        return false;
                    }
                }
                return true;
            }

            void VulkanRenderDevice::CleanupPerDrawBuffers()
            {
                for (int f = 0; f < kMaxFramesInFlight; ++f) {
                    if (perDrawMapped[f] && perDrawMemories[f]) {
                        vkUnmapMemory(device, perDrawMemories[f]);
                        perDrawMapped[f] = nullptr;
                    }
                    if (perDrawBuffers[f]) {
                        vkDestroyBuffer(device, perDrawBuffers[f], nullptr);
                        perDrawBuffers[f] = VK_NULL_HANDLE;
                    }
                    if (perDrawMemories[f]) {
                        vkFreeMemory(device, perDrawMemories[f], nullptr);
                        perDrawMemories[f] = VK_NULL_HANDLE;
                    }
                }
            }

            bool VulkanRenderDevice::CreateFallbackResources()
            {
                if (!CreateBufferRaw(fallbackUBOSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        fallbackUBOBuffer, fallbackUBOMemory)) {
                    return false;
                }
                std::vector<std::uint8_t> zeros(static_cast<std::size_t>(fallbackUBOSize), 0);
                UploadHostVisibleBuffer(fallbackUBOMemory, zeros.data(), zeros.size());

                const std::uint8_t whiteRGBA[4] = { 255, 255, 255, 255 };
                fallbackTexture2D = {};
                if (!CreateImageRaw(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 0,
                        fallbackTexture2D.image, fallbackTexture2D.memory)) {
                    return false;
                }
                fallbackTexture2D.view = CreateImageViewRaw(fallbackTexture2D.image, VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
                fallbackTexture2D.width = 1;
                fallbackTexture2D.height = 1;
                fallbackTexture2D.format = VK_FORMAT_R8G8B8A8_UNORM;
                {
                    VkBuffer staging = VK_NULL_HANDLE;
                    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                    CreateBufferRaw(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        staging, stagingMem);
                    UploadHostVisibleBuffer(stagingMem, whiteRGBA, 4);
                    TransitionImageLayout(fallbackTexture2D.image, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
                    CopyBufferToImage(staging, fallbackTexture2D.image, 1, 1, 0);
                    TransitionImageLayout(fallbackTexture2D.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
                    vkDestroyBuffer(device, staging, nullptr);
                    vkFreeMemory(device, stagingMem, nullptr);
                }
                RecreateSampler(fallbackTexture2D);

                // Zeroed vec4 used as dummy vertex input for optional shader locations.
                {
                    const float zeroVec4[4] = { 0.f, 0.f, 0.f, 0.f };
                    if (!CreateBufferRaw(sizeof(zeroVec4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            fallbackVertexBuffer, fallbackVertexMemory)) {
                        return false;
                    }
                    UploadHostVisibleBuffer(fallbackVertexMemory, zeroVec4, sizeof(zeroVec4));
                }

                fallbackCubemap = {};
                fallbackCubemap.isCubemap = true;
                fallbackCubemap.layerCount = 6;
                if (!CreateImageRaw(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                        fallbackCubemap.image, fallbackCubemap.memory)) {
                    return false;
                }
                fallbackCubemap.view = CreateImageViewRaw(fallbackCubemap.image, VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6);
                fallbackCubemap.width = 1;
                fallbackCubemap.height = 1;
                fallbackCubemap.format = VK_FORMAT_R8G8B8A8_UNORM;
                TransitionImageLayout(fallbackCubemap.image, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6);
                for (std::uint32_t face = 0; face < 6; ++face) {
                    VkBuffer staging = VK_NULL_HANDLE;
                    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                    CreateBufferRaw(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        staging, stagingMem);
                    UploadHostVisibleBuffer(stagingMem, whiteRGBA, 4);
                    CopyBufferToImage(staging, fallbackCubemap.image, 1, 1, face);
                    vkDestroyBuffer(device, staging, nullptr);
                    vkFreeMemory(device, stagingMem, nullptr);
                }
                TransitionImageLayout(fallbackCubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6);
                RecreateSampler(fallbackCubemap);
                return true;
            }

            void VulkanRenderDevice::CleanupFallbackResources()
            {
                auto destroyTex = [&](TextureResource& t) {
                    if (t.sampler) vkDestroySampler(device, t.sampler, nullptr);
                    if (t.view) vkDestroyImageView(device, t.view, nullptr);
                    if (t.image) vkDestroyImage(device, t.image, nullptr);
                    if (t.memory) vkFreeMemory(device, t.memory, nullptr);
                    t = {};
                };
                destroyTex(fallbackTexture2D);
                destroyTex(fallbackCubemap);
                if (fallbackVertexBuffer) {
                    vkDestroyBuffer(device, fallbackVertexBuffer, nullptr);
                    fallbackVertexBuffer = VK_NULL_HANDLE;
                }
                if (fallbackVertexMemory) {
                    vkFreeMemory(device, fallbackVertexMemory, nullptr);
                    fallbackVertexMemory = VK_NULL_HANDLE;
                }
                if (fallbackUBOBuffer) {
                    vkDestroyBuffer(device, fallbackUBOBuffer, nullptr);
                    fallbackUBOBuffer = VK_NULL_HANDLE;
                }
                if (fallbackUBOMemory) {
                    vkFreeMemory(device, fallbackUBOMemory, nullptr);
                    fallbackUBOMemory = VK_NULL_HANDLE;
                }
            }

            VkFormat VulkanRenderDevice::FindSupportedDepthFormat() const
            {
                const VkFormat candidates[] = {
                    VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
                };
                for (VkFormat format : candidates) {
                    VkFormatProperties props{};
                    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
                    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                        return format;
                    }
                }
                return VK_FORMAT_D32_SFLOAT;
            }

            bool VulkanRenderDevice::CreateDepthResources()
            {
                CleanupDepthResources();
                depthFormat = FindSupportedDepthFormat();
                depthImages.resize(swapchainImages.size());
                depthImageMemories.resize(swapchainImages.size());
                depthImageViews.resize(swapchainImages.size());
                for (std::size_t i = 0; i < swapchainImages.size(); ++i) {
                    if (!CreateImageRaw(swapchainExtent.width, swapchainExtent.height, depthFormat,
                            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 0,
                            depthImages[i], depthImageMemories[i])) {
                        return false;
                    }
                    depthImageViews[i] = CreateImageViewRaw(depthImages[i], depthFormat,
                        VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
                    TransitionImageLayout(depthImages[i], VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
                }
                return true;
            }

            void VulkanRenderDevice::CleanupDepthResources()
            {
                for (VkImageView v : depthImageViews) {
                    if (v) vkDestroyImageView(device, v, nullptr);
                }
                depthImageViews.clear();
                for (VkImage img : depthImages) {
                    if (img) vkDestroyImage(device, img, nullptr);
                }
                depthImages.clear();
                for (VkDeviceMemory mem : depthImageMemories) {
                    if (mem) vkFreeMemory(device, mem, nullptr);
                }
                depthImageMemories.clear();
            }

            // ---------------------------------------------------------------------------
            // Instance / device / swapchain bootstrap
            // ---------------------------------------------------------------------------

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
                    if (!found) return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateInstance()
            {
                const bool useValidation = kEnableValidationLayers && CheckValidationLayerSupport();
                if (kEnableValidationLayers && !useValidation) {
                    RTB_WARN("VulkanRenderDevice: validation layers requested but not available");
                }

                VkApplicationInfo appInfo{};
                appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                appInfo.pApplicationName = "RTBEngine Application";
                appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
                appInfo.pEngineName = "RTBEngine";
                appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
                appInfo.apiVersion = VK_API_VERSION_1_2;
                vulkanApiVersion = VK_API_VERSION_1_2;

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
                if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateInstance failed");
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::SetupDebugMessenger()
            {
                if (!kEnableValidationLayers || !debugUtilsAvailable) return true;
                VkDebugUtilsMessengerCreateInfoEXT createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                createInfo.pfnUserCallback = DebugCallback;
                if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
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
                    if (indices.IsComplete()) break;
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
                if (!FindQueueFamilies(candidate).IsComplete()) return false;
                if (!CheckDeviceExtensionSupport(candidate)) return false;
                const SwapChainSupportDetails support = QuerySwapChainSupport(candidate);
                return !support.formats.empty() && !support.presentModes.empty();
            }

            bool VulkanRenderDevice::PickPhysicalDevice()
            {
                std::uint32_t deviceCount = 0;
                vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
                if (deviceCount == 0) {
                    RTB_ERROR("VulkanRenderDevice: no Vulkan-capable GPUs");
                    return false;
                }
                std::vector<VkPhysicalDevice> devices(deviceCount);
                vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
                VkPhysicalDevice best = VK_NULL_HANDLE;
                int bestScore = -1;
                for (VkPhysicalDevice candidate : devices) {
                    if (!IsDeviceSuitable(candidate)) continue;
                    VkPhysicalDeviceProperties props{};
                    vkGetPhysicalDeviceProperties(candidate, &props);
                    int score = 1;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 100;
                    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 50;
                    if (score > bestScore) { bestScore = score; best = candidate; }
                }
                if (!best) {
                    RTB_ERROR("VulkanRenderDevice: no suitable physical device");
                    return false;
                }
                physicalDevice = best;
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(physicalDevice, &props);
                RTB_INFO(std::string("VulkanRenderDevice: selected '") + props.deviceName + "'");
                return true;
            }

            bool VulkanRenderDevice::CreateLogicalDevice()
            {
                const QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
                graphicsQueueFamily = indices.graphicsFamily;
                presentQueueFamily = indices.presentFamily;
                const std::set<std::uint32_t> unique = { graphicsQueueFamily, presentQueueFamily };
                std::vector<VkDeviceQueueCreateInfo> queueInfos;
                const float priority = 1.0f;
                for (std::uint32_t family : unique) {
                    VkDeviceQueueCreateInfo qi{};
                    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    qi.queueFamilyIndex = family;
                    qi.queueCount = 1;
                    qi.pQueuePriorities = &priority;
                    queueInfos.push_back(qi);
                }

                enabledDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
                std::uint32_t extCount = 0;
                vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
                std::vector<VkExtensionProperties> availableExts(extCount);
                vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExts.data());
                auto hasExt = [&](const char* name) {
                    for (const auto& e : availableExts) {
                        if (std::strcmp(e.extensionName, name) == 0) return true;
                    }
                    return false;
                };
                const char* rtExts[] = {
                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                    VK_KHR_RAY_QUERY_EXTENSION_NAME,
                    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
                };
                rayQuerySupported = true;
                for (const char* ext : rtExts) {
                    if (!hasExt(ext)) {
                        rayQuerySupported = false;
                        break;
                    }
                }
                if (rayQuerySupported) {
                    for (const char* ext : rtExts) {
                        enabledDeviceExtensions.push_back(ext);
                    }
                }

                VkPhysicalDeviceFeatures features{};
                VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
                bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
                bufferDeviceAddressFeatures.bufferDeviceAddress = rayQuerySupported ? VK_TRUE : VK_FALSE;

                VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
                accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                accelerationStructureFeatures.accelerationStructure = rayQuerySupported ? VK_TRUE : VK_FALSE;

                VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
                rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
                rayQueryFeatures.rayQuery = rayQuerySupported ? VK_TRUE : VK_FALSE;

                void* pNext = nullptr;
                if (rayQuerySupported) {
                    bufferDeviceAddressFeatures.pNext = &accelerationStructureFeatures;
                    accelerationStructureFeatures.pNext = &rayQueryFeatures;
                    pNext = &bufferDeviceAddressFeatures;
                }

                VkDeviceCreateInfo ci{};
                ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                ci.pNext = pNext;
                ci.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size());
                ci.pQueueCreateInfos = queueInfos.data();
                ci.pEnabledFeatures = &features;
                ci.enabledExtensionCount = static_cast<std::uint32_t>(enabledDeviceExtensions.size());
                ci.ppEnabledExtensionNames = enabledDeviceExtensions.data();
                if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateDevice failed");
                    return false;
                }
                vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
                vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
                if (rayQuerySupported) {
                    RTB_INFO("VulkanRenderDevice: ray query extensions enabled");
                }
                return true;
            }

            VkSurfaceFormatKHR VulkanRenderDevice::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
            {
                // Prefer UNORM so ImGui (and other linear-ish UI writes) match OpenGL appearance.
                // SRGB swapchains cause the editor UI to look washed / too bright.
                for (const auto& f : formats) {
                    if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
                }
                for (const auto& f : formats) {
                    if (f.format == VK_FORMAT_B8G8R8A8_UNORM) return f;
                }
                for (const auto& f : formats) {
                    if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
                }
                return formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
            }

            VkPresentModeKHR VulkanRenderDevice::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const
            {
                if (!vSyncEnabled) {
                    for (VkPresentModeKHR m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
                    for (VkPresentModeKHR m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
                }
                return VK_PRESENT_MODE_FIFO_KHR;
            }

            VkExtent2D VulkanRenderDevice::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
            {
                if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
                    return capabilities.currentExtent;
                }
                int width = 0, height = 0;
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
                    swapchain = VK_NULL_HANDLE;
                    return true;
                }
                std::uint32_t imageCount = support.capabilities.minImageCount + 1;
                if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
                    imageCount = support.capabilities.maxImageCount;
                }
                VkSwapchainCreateInfoKHR ci{};
                ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                ci.surface = surface;
                ci.minImageCount = imageCount;
                ci.imageFormat = surfaceFormat.format;
                ci.imageColorSpace = surfaceFormat.colorSpace;
                ci.imageExtent = extent;
                ci.imageArrayLayers = 1;
                ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                const std::uint32_t qfi[] = { graphicsQueueFamily, presentQueueFamily };
                if (graphicsQueueFamily != presentQueueFamily) {
                    ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                    ci.queueFamilyIndexCount = 2;
                    ci.pQueueFamilyIndices = qfi;
                }
                else {
                    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                }
                ci.preTransform = support.capabilities.currentTransform;
                ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                ci.presentMode = presentMode;
                ci.clipped = VK_TRUE;
                if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateSwapchainKHR failed");
                    return false;
                }
                std::uint32_t actual = 0;
                vkGetSwapchainImagesKHR(device, swapchain, &actual, nullptr);
                swapchainImages.resize(actual);
                vkGetSwapchainImagesKHR(device, swapchain, &actual, swapchainImages.data());
                return true;
            }

            bool VulkanRenderDevice::CreateImageViews()
            {
                swapchainImageViews.resize(swapchainImages.size());
                for (std::size_t i = 0; i < swapchainImages.size(); ++i) {
                    swapchainImageViews[i] = CreateImageViewRaw(swapchainImages[i], swapchainImageFormat,
                        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
                    if (!swapchainImageViews[i]) return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateRenderPass()
            {
                if (renderPass) {
                    vkDestroyRenderPass(device, renderPass, nullptr);
                    renderPass = VK_NULL_HANDLE;
                }
                depthFormat = FindSupportedDepthFormat();

                VkAttachmentDescription color{};
                color.format = swapchainImageFormat;
                color.samples = VK_SAMPLE_COUNT_1_BIT;
                color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                VkAttachmentDescription depth{};
                depth.format = depthFormat;
                depth.samples = VK_SAMPLE_COUNT_1_BIT;
                depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
                VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

                VkSubpassDescription subpass{};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorRef;
                subpass.pDepthStencilAttachment = &depthRef;

                VkSubpassDependency dep{};
                dep.srcSubpass = VK_SUBPASS_EXTERNAL;
                dep.dstSubpass = 0;
                dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                const VkAttachmentDescription attachments[] = { color, depth };
                VkRenderPassCreateInfo rp{};
                rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                rp.attachmentCount = 2;
                rp.pAttachments = attachments;
                rp.subpassCount = 1;
                rp.pSubpasses = &subpass;
                rp.dependencyCount = 1;
                rp.pDependencies = &dep;
                if (vkCreateRenderPass(device, &rp, nullptr, &renderPass) != VK_SUCCESS) {
                    RTB_ERROR("VulkanRenderDevice: vkCreateRenderPass failed");
                    return false;
                }
                return true;
            }

            bool VulkanRenderDevice::CreateFramebuffers()
            {
                swapchainFramebuffers.resize(swapchainImageViews.size());
                for (std::size_t i = 0; i < swapchainImageViews.size(); ++i) {
                    const VkImageView attachments[] = { swapchainImageViews[i], depthImageViews[i] };
                    VkFramebufferCreateInfo fi{};
                    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    fi.renderPass = renderPass;
                    fi.attachmentCount = 2;
                    fi.pAttachments = attachments;
                    fi.width = swapchainExtent.width;
                    fi.height = swapchainExtent.height;
                    fi.layers = 1;
                    if (vkCreateFramebuffer(device, &fi, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                        RTB_ERROR("VulkanRenderDevice: vkCreateFramebuffer failed");
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateCommandPool()
            {
                VkCommandPoolCreateInfo pi{};
                pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                pi.queueFamilyIndex = graphicsQueueFamily;
                return vkCreateCommandPool(device, &pi, nullptr, &commandPool) == VK_SUCCESS;
            }

            bool VulkanRenderDevice::CreateCommandBuffers()
            {
                commandBuffers.resize(kMaxFramesInFlight);
                VkCommandBufferAllocateInfo ai{};
                ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                ai.commandPool = commandPool;
                ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                ai.commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size());
                return vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) == VK_SUCCESS;
            }

            bool VulkanRenderDevice::CreateSyncObjects()
            {
                imageAvailableSemaphores.resize(kMaxFramesInFlight);
                inFlightFences.resize(kMaxFramesInFlight);
                VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                for (int i = 0; i < kMaxFramesInFlight; ++i) {
                    if (vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
                        || vkCreateFence(device, &fi, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                        return false;
                    }
                }
                return true;
            }

            bool VulkanRenderDevice::CreateSwapchainImageSemaphores()
            {
                for (VkSemaphore s : renderFinishedSemaphores) {
                    if (s) vkDestroySemaphore(device, s, nullptr);
                }
                renderFinishedSemaphores.clear();
                imagesInFlight.assign(swapchainImages.size(), VK_NULL_HANDLE);
                if (swapchainImages.empty()) return false;
                renderFinishedSemaphores.resize(swapchainImages.size(), VK_NULL_HANDLE);
                VkSemaphoreCreateInfo si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
                for (std::size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
                    if (vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                        return false;
                    }
                }
                return true;
            }

            void VulkanRenderDevice::CleanupSwapchain()
            {
                for (VkFramebuffer fb : swapchainFramebuffers) {
                    if (fb) vkDestroyFramebuffer(device, fb, nullptr);
                }
                swapchainFramebuffers.clear();
                for (VkImageView v : swapchainImageViews) {
                    if (v) vkDestroyImageView(device, v, nullptr);
                }
                swapchainImageViews.clear();
                for (VkSemaphore s : renderFinishedSemaphores) {
                    if (s) vkDestroySemaphore(device, s, nullptr);
                }
                renderFinishedSemaphores.clear();
                imagesInFlight.clear();
                if (swapchain) {
                    vkDestroySwapchainKHR(device, swapchain, nullptr);
                    swapchain = VK_NULL_HANDLE;
                }
                swapchainImages.clear();
            }

            bool VulkanRenderDevice::RecreateSwapchain()
            {
                int width = 0, height = 0;
                SDL_Vulkan_GetDrawableSize(window, &width, &height);
                if (width == 0 || height == 0) return false;
                vkDeviceWaitIdle(device);
                DestroyAllPipelines();
                CleanupSwapchain();
                CleanupDepthResources();
                if (!CreateSwapchain() || swapchain == VK_NULL_HANDLE) return false;
                if (!CreateSwapchainImageSemaphores() || !CreateImageViews()
                    || !CreateDepthResources() || !CreateFramebuffers()) {
                    return false;
                }
                pendingViewport[2] = static_cast<int>(swapchainExtent.width);
                pendingViewport[3] = static_cast<int>(swapchainExtent.height);

                if (imguiBackendInitialized) {
                    // renderPass itself is not recreated here (only destroyed on full
                    // Shutdown), so the ImGui pipeline stays valid -- just refresh the
                    // image count the backend tracks internally.
                    ImGui_ImplVulkan_SetMinImageCount(2);
                }
                return true;
            }

        }
    }
}

// GI / compute extensions for VulkanRenderDevice
namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            GiCapabilities VulkanRenderDevice::GetGiCapabilities() const
            {
                GiCapabilities caps{};
                caps.computeShaders = true;
                caps.storageBuffers = true;
                caps.storageImages = true;
                caps.texture3D = true;
                caps.rayQuery = rayQuerySupported && giContext && giContext->IsRayQueryAvailable();
                return caps;
            }

            void VulkanRenderDevice::ExecuteOneShotCommand(const std::function<void(VkCommandBuffer)>& recordFn)
            {
                if (!initialized || !recordFn) return;
                VkCommandBuffer cmd = BeginSingleTimeCommands();
                if (!cmd) return;
                recordFn(cmd);
                EndSingleTimeCommands(cmd);
            }

            void VulkanRenderDevice::CreateDeviceLocalBufferRaw(VkDeviceSize size, VkBufferUsageFlags usage,
                                                                VkBuffer& outBuffer, VkDeviceMemory& outMemory)
            {
                outBuffer = VK_NULL_HANDLE;
                outMemory = VK_NULL_HANDLE;
                if (size == 0) return;
                VkBufferUsageFlags fullUsage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
                CreateBufferRaw(size, fullUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outBuffer, outMemory);
            }

            void VulkanRenderDevice::UploadToDeviceLocalBuffer(VkBuffer buffer, const void* data, std::size_t size)
            {
                if (!buffer || !data || size == 0) return;
                VkBuffer staging = VK_NULL_HANDLE;
                VkDeviceMemory stagingMem = VK_NULL_HANDLE;
                CreateBufferRaw(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    staging, stagingMem);
                UploadHostVisibleBuffer(stagingMem, data, size);
                VkCommandBuffer cmd = BeginSingleTimeCommands();
                VkBufferCopy region{};
                region.size = size;
                vkCmdCopyBuffer(cmd, staging, buffer, 1, &region);
                EndSingleTimeCommands(cmd);
                vkDestroyBuffer(device, staging, nullptr);
                vkFreeMemory(device, stagingMem, nullptr);
            }

            VkBuffer VulkanRenderDevice::GetBufferHandle(GpuId id) const
            {
                return ResolveBufferHandle(id);
            }

            VkImageView VulkanRenderDevice::GetTextureImageView(GpuId id) const
            {
                auto it = textures.find(id);
                return (it != textures.end()) ? it->second.view : VK_NULL_HANDLE;
            }

            GpuId VulkanRenderDevice::CreateStorageImage2DInternal(int width, int height, TextureFormat format)
            {
                const GpuId id = nextId++;
                TextureResource res{};
                VkFormat vkFmt = VK_FORMAT_R16G16B16A16_SFLOAT;
                if (format == TextureFormat::R32F) vkFmt = VK_FORMAT_R32_SFLOAT;
                if (!CreateImageRaw(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), vkFmt,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 0, res.image, res.memory)) {
                    return kInvalidGpuId;
                }
                res.view = CreateImageViewRaw(res.image, vkFmt, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
                // LINEAR within tile; SampleDDGI insets UVs so neighboring probe tiles do not bleed.
                res.sampler = CreateSamplerRaw(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
                res.width = width;
                res.height = height;
                res.format = vkFmt;
                textures[id] = res;
                TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1);
                textures[id].currentLayout = VK_IMAGE_LAYOUT_GENERAL;
                return id;
            }

            void VulkanRenderDevice::ClearStorageImage2D(GpuId texture, float r, float g, float b, float a)
            {
                auto it = textures.find(texture);
                if (it == textures.end() || !it->second.image) return;
                TextureResource& res = it->second;
                TransitionImageLayout(res.image, res.currentLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1);
                ExecuteOneShotCommand([&](VkCommandBuffer cmd) {
                    VkClearColorValue clear{};
                    clear.float32[0] = r;
                    clear.float32[1] = g;
                    clear.float32[2] = b;
                    clear.float32[3] = a;
                    VkImageSubresourceRange range{};
                    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    range.levelCount = 1;
                    range.layerCount = 1;
                    vkCmdClearColorImage(cmd, res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
                });
                TransitionImageLayout(res.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1);
                res.currentLayout = VK_IMAGE_LAYOUT_GENERAL;
            }

            void VulkanRenderDevice::UpdateStorageImageLayout(GpuId texture, VkImageLayout layout)
            {
                auto it = textures.find(texture);
                if (it == textures.end() || !it->second.image) return;
                if (it->second.currentLayout == layout) {
                    return;
                }
                TransitionImageLayout(it->second.image, it->second.currentLayout, layout,
                    VK_IMAGE_ASPECT_COLOR_BIT, 1);
                it->second.currentLayout = layout;
            }

            void VulkanRenderDevice::MemoryBarrierComputeToGraphicsInternal()
            {
                ExecuteOneShotCommand([](VkCommandBuffer cmd) {
                    VkMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
                });
            }

            GpuId VulkanRenderDevice::CreateStorageImage2D(int width, int height, TextureFormat format)
            {
                return CreateStorageImage2DInternal(width, height, format);
            }

            void VulkanRenderDevice::BindStorageImage2D(GpuId texture, unsigned int bindingPoint, StorageAccess access)
            {
                (void)access;
                if (bindingPoint == kDDGIIrradianceBinding) boundDDGIIrradiance = texture;
                else if (bindingPoint == kDDGIDistanceBinding) boundDDGIDistance = texture;
            }

            void VulkanRenderDevice::MemoryBarrierComputeToGraphics()
            {
                MemoryBarrierComputeToGraphicsInternal();
            }

            GpuId VulkanRenderDevice::CreateDeviceLocalBuffer(const void* data, std::size_t size, bool indexBuffer)
            {
                if (!giContext || size == 0) return kInvalidGpuId;
                VkBufferUsageFlags usage = indexBuffer ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                return giContext->CreateDeviceLocalBuffer(data, size, usage);
            }

            std::uint64_t VulkanRenderDevice::GetBufferDeviceAddress(GpuId buffer) const
            {
                return giContext ? giContext->GetBufferDeviceAddress(buffer) : 0;
            }

            GpuId VulkanRenderDevice::CreateComputeProgram(const std::string& computeSource)
            {
                return giContext ? giContext->CreateComputeProgram(computeSource) : kInvalidGpuId;
            }

            void VulkanRenderDevice::DestroyComputeProgram(GpuId program)
            {
                if (giContext) giContext->DestroyComputeProgram(program);
            }

            void VulkanRenderDevice::BindComputeProgram(GpuId program)
            {
                if (giContext) giContext->BindComputeProgram(program);
            }

            void VulkanRenderDevice::DispatchCompute(unsigned int groupCountX, unsigned int groupCountY, unsigned int groupCountZ)
            {
                if (giContext) giContext->DispatchCompute(kInvalidGpuId, groupCountX, groupCountY, groupCountZ);
            }

            GpuId VulkanRenderDevice::CreateStorageBuffer(std::size_t size)
            {
                return giContext ? giContext->CreateStorageBuffer(size) : kInvalidGpuId;
            }

            void VulkanRenderDevice::DestroyStorageBuffer(GpuId buffer)
            {
                (void)buffer;
            }

            void VulkanRenderDevice::UpdateStorageBuffer(GpuId buffer, const void* data, std::size_t size, std::size_t offset)
            {
                if (giContext) giContext->UpdateStorageBuffer(buffer, data, size, offset);
            }

            void VulkanRenderDevice::BindStorageBuffer(GpuId buffer, unsigned int bindingPoint)
            {
                if (giContext) giContext->BindStorageBuffer(buffer, bindingPoint);
            }

            GpuId VulkanRenderDevice::CreateTexture3D()
            {
                const GpuId id = nextId++;
                textures[id] = {};
                return id;
            }

            void VulkanRenderDevice::DestroyTexture3D(GpuId texture)
            {
                DestroyTexture(texture);
            }

            void VulkanRenderDevice::SetTexture3DData(GpuId texture, TextureFormat format, int width, int height, int depth,
                                                      const void* pixels)
            {
                auto it = textures.find(texture);
                if (it == textures.end()) return;
                bool isDepth = false;
                VkFormat vkFmt = ToVkFormat(format, isDepth);
                (void)isDepth;
                if (!CreateImage3DRaw(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                        static_cast<std::uint32_t>(depth), vkFmt, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, it->second.image, it->second.memory)) {
                    return;
                }
                it->second.view = CreateImageViewRaw(it->second.image, vkFmt, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_VIEW_TYPE_3D, static_cast<std::uint32_t>(depth));
                it->second.sampler = CreateSamplerRaw(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, false);
                it->second.width = width;
                it->second.height = height;
                it->second.layerCount = static_cast<std::uint32_t>(depth);
                if (pixels) {
                    (void)pixels;
                }
            }

            void VulkanRenderDevice::BindTexture3D(GpuId texture, unsigned int slot)
            {
                boundTextureSlots[slot] = texture;
            }

        }
    }
}
