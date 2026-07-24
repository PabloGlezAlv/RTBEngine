#pragma once

#include "../../RTBEngineAPI.h"
#include "GraphicsAPI.h"
#include "RenderTypes.h"
#include <cstddef>
#include <cstdint>
#include <string>

struct SDL_Window;
struct ImDrawData;

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            class RTB_API IRenderDevice {
            public:
                virtual ~IRenderDevice() = default;

                virtual bool Initialize(SDL_Window* window, bool vSync) = 0;
                virtual void Shutdown() = 0;
                virtual GraphicsAPI GetAPI() const = 0;

                virtual void MakeCurrent() = 0;
                virtual void Present() = 0;
                virtual void SetVSync(bool enabled) = 0;
                virtual void* GetNativeContext() const = 0;

                // Frame / raster state
                virtual void SetViewport(int x, int y, int width, int height) = 0;
                virtual void SetClearColor(float r, float g, float b, float a = 1.0f) = 0;
                virtual void Clear(ClearMask mask) = 0;
                virtual void SetDepthTest(bool enabled) = 0;
                virtual void SetDepthFunc(DepthFunc func) = 0;
                virtual void SetDepthWrite(bool enabled) = 0;
                virtual void SetCullFace(bool enabled) = 0;
                virtual void SetBlend(bool enabled) = 0;
                virtual void SetBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha) = 0;
                virtual void SetColorMask(bool red, bool green, bool blue, bool alpha) = 0;

                // Shader programs
                virtual GpuId CreateShaderProgram(const std::string& vertexSource,
                                                  const std::string& fragmentSource) = 0;
                virtual void DestroyShaderProgram(GpuId program) = 0;
                virtual void BindShaderProgram(GpuId program) = 0;
                virtual int GetUniformLocation(GpuId program, const char* name) = 0;
                virtual void SetUniformBool(int location, bool value) = 0;
                virtual void SetUniformInt(int location, int value) = 0;
                virtual void SetUniformFloat(int location, float value) = 0;
                virtual void SetUniformVec2(int location, float x, float y) = 0;
                virtual void SetUniformVec3(int location, float x, float y, float z) = 0;
                virtual void SetUniformVec4(int location, float x, float y, float z, float w) = 0;
                virtual void SetUniformMat4(int location, const float* matrix4x4) = 0;
                virtual void BindUniformBlock(GpuId program, const char* blockName, unsigned int bindingPoint) = 0;

                // Buffers / UBOs
                virtual GpuId CreateBuffer() = 0;
                virtual void DestroyBuffer(GpuId buffer) = 0;
                virtual void SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) = 0;
                virtual void UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size) = 0;
                virtual void BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint) = 0;

                // Textures (2D)
                virtual GpuId CreateTexture2D() = 0;
                virtual void DestroyTexture(GpuId texture) = 0;
                virtual void SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                             const void* pixels, bool generateMipmaps) = 0;
                virtual void SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter) = 0;
                virtual void SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT) = 0;
                virtual void SetTexture2DDepthShadowParams(GpuId texture) = 0;
                virtual void BindTexture2D(GpuId texture, unsigned int slot) = 0;
                virtual void UnbindTexture2D() = 0;

                // Cubemap
                virtual GpuId CreateCubemap() = 0;
                virtual void SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                           int width, int height, const void* pixels) = 0;
                virtual void SetCubemapFilterWrap(GpuId cubemap) = 0;
                virtual void BindCubemap(GpuId cubemap, unsigned int slot) = 0;

                // Framebuffers
                virtual GpuId CreateFramebuffer() = 0;
                virtual void DestroyFramebuffer(GpuId framebuffer) = 0;
                virtual void BindFramebuffer(GpuId framebuffer) = 0;
                virtual void UnbindFramebuffer() = 0;
                virtual void AttachFramebufferColorTexture(GpuId framebuffer, GpuId texture) = 0;
                virtual void AttachFramebufferDepthTexture(GpuId framebuffer, GpuId texture) = 0;
                virtual void SetFramebufferDrawReadNone() = 0;
                virtual bool IsFramebufferComplete() const = 0;
                virtual GpuId CreateColorTextureForFramebuffer(int width, int height) = 0;
                virtual GpuId CreateDepthTextureForFramebuffer(int width, int height) = 0;

                // Mesh / VAO (standard Vertex layout used by Engine::Mesh)
                virtual GpuId CreateVertexArray() = 0;
                virtual void DestroyVertexArray(GpuId vao) = 0;
                virtual void BindVertexArray(GpuId vao) = 0;
                virtual void UnbindVertexArray() = 0;
                virtual void SetArrayBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) = 0;
                virtual void SetElementBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) = 0;
                virtual void BindArrayBuffer(GpuId buffer) = 0;
                virtual void BindElementBuffer(GpuId buffer) = 0;
                virtual void EnableVertexAttribFloat(unsigned int location, int components, int stride, std::size_t offset) = 0;
                virtual void EnableVertexAttribInt(unsigned int location, int components, int stride, std::size_t offset) = 0;
                virtual void SetVertexAttribDivisor(unsigned int location, unsigned int divisor) = 0;
                virtual void DrawIndexed(PrimitiveTopology topology, int indexCount, IndexType indexType) = 0;
                virtual void DrawIndexedInstanced(PrimitiveTopology topology, int indexCount, IndexType indexType, int instanceCount) = 0;
                virtual void DrawArrays(PrimitiveTopology topology, int first, int count) = 0;
                virtual void DrawArraysInstanced(PrimitiveTopology topology, int first, int count, int instanceCount) = 0;

                // ImGui lifecycle (platform + renderer backends). Call after ImGui::CreateContext().
                virtual bool InitializeImGuiBackend(SDL_Window* window) = 0;
                virtual void ShutdownImGuiBackend() = 0;
                virtual void BeginImGuiFrame() = 0;
                // OpenGL: renders immediately. Vulkan: queues for Present() inside the swapchain pass.
                virtual void QueueImGuiDrawData(ImDrawData* drawData) = 0;

                // ImGui texture bridge. OpenGL: GL texture name. Vulkan: VkDescriptorSet as uintptr_t.
                virtual std::uintptr_t GetNativeTextureIdForImGui(GpuId texture) const = 0;

                // GI / compute capabilities
                virtual GiCapabilities GetGiCapabilities() const = 0;

                // Compute shaders
                virtual GpuId CreateComputeProgram(const std::string& computeSource) = 0;
                virtual void DestroyComputeProgram(GpuId program) = 0;
                virtual void BindComputeProgram(GpuId program) = 0;
                virtual void DispatchCompute(unsigned int groupCountX, unsigned int groupCountY, unsigned int groupCountZ) = 0;

                // Storage buffers (SSBO)
                virtual GpuId CreateStorageBuffer(std::size_t size) = 0;
                virtual void DestroyStorageBuffer(GpuId buffer) = 0;
                virtual void UpdateStorageBuffer(GpuId buffer, const void* data, std::size_t size, std::size_t offset = 0) = 0;
                virtual void BindStorageBuffer(GpuId buffer, unsigned int bindingPoint) = 0;

                // 3D textures
                virtual GpuId CreateTexture3D() = 0;
                virtual void DestroyTexture3D(GpuId texture) = 0;
                virtual void SetTexture3DData(GpuId texture, TextureFormat format, int width, int height, int depth,
                                            const void* pixels) = 0;
                virtual void BindTexture3D(GpuId texture, unsigned int slot) = 0;

                // Storage images (2D)
                virtual GpuId CreateStorageImage2D(int width, int height, TextureFormat format) = 0;
                virtual void BindStorageImage2D(GpuId texture, unsigned int bindingPoint, StorageAccess access) = 0;
                virtual void ClearStorageImage2D(GpuId texture, float r, float g, float b, float a) = 0;

                // GPU memory barrier between compute and graphics passes
                virtual void MemoryBarrierComputeToGraphics() = 0;

                // Device-local geometry buffer for ray tracing (Vulkan BLAS source)
                virtual GpuId CreateDeviceLocalBuffer(const void* data, std::size_t size, bool indexBuffer) = 0;
                virtual std::uint64_t GetBufferDeviceAddress(GpuId buffer) const = 0;
            };

        }
    }
}
