#pragma once

#include "../IRenderDevice.h"
#include <SDL.h>
#include <cstdint>

struct ImDrawData;

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            class OpenGLRenderDevice final : public IRenderDevice {
            public:
                OpenGLRenderDevice() = default;
                ~OpenGLRenderDevice() override;

                bool Initialize(SDL_Window* window, bool vSync) override;
                void Shutdown() override;
                GraphicsAPI GetAPI() const override { return GraphicsAPI::OpenGL; }

                void MakeCurrent() override;
                void Present() override;
                void SetVSync(bool enabled) override;
                void* GetNativeContext() const override { return glContext; }

                void SetViewport(int x, int y, int width, int height) override;
                void SetClearColor(float r, float g, float b, float a) override;
                void Clear(ClearMask mask) override;
                void SetDepthTest(bool enabled) override;
                void SetDepthFunc(DepthFunc func) override;
                void SetDepthWrite(bool enabled) override;
                void SetCullFace(bool enabled) override;
                void SetBlend(bool enabled) override;
                void SetBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha) override;
                void SetColorMask(bool red, bool green, bool blue, bool alpha) override;

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

                GpuId CreateBuffer() override;
                void DestroyBuffer(GpuId buffer) override;
                void SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage) override;
                void UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size) override;
                void BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint) override;

                GpuId CreateTexture2D() override;
                void DestroyTexture(GpuId texture) override;
                void SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                     const void* pixels, bool generateMipmaps) override;
                void SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter) override;
                void SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT) override;
                void SetTexture2DDepthShadowParams(GpuId texture) override;
                void BindTexture2D(GpuId texture, unsigned int slot) override;
                void UnbindTexture2D() override;

                GpuId CreateCubemap() override;
                void SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                   int width, int height, const void* pixels) override;
                void SetCubemapFilterWrap(GpuId cubemap) override;
                void BindCubemap(GpuId cubemap, unsigned int slot) override;

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

            private:
                static unsigned int ToGLDepthFunc(DepthFunc func);
                static unsigned int ToGLBufferUsage(BufferUsage usage);
                static unsigned int ToGLTopology(PrimitiveTopology topology);
                static unsigned int ToGLIndexType(IndexType type);
                static unsigned int ToGLFilter(TextureFilter filter);
                static unsigned int ToGLWrap(TextureWrap wrap);
                static void ResolveTextureFormat(TextureFormat format, unsigned int& internalFormat,
                                                 unsigned int& pixelFormat, unsigned int& pixelType);

                SDL_Window* window = nullptr;
                SDL_GLContext glContext = nullptr;
                bool initialized = false;
                bool imguiBackendInitialized = false;
            };

        }
    }
}
