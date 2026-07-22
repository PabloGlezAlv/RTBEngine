#include "OpenGLRenderDevice.h"
#include "../../../Core/Logger.h"
#include <GL/glew.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            OpenGLRenderDevice::~OpenGLRenderDevice()
            {
                Shutdown();
            }

            bool OpenGLRenderDevice::Initialize(SDL_Window* sdlWindow, bool vSync)
            {
                if (initialized) {
                    return true;
                }

                if (!sdlWindow) {
                    RTB_ERROR("OpenGLRenderDevice::Initialize - null SDL_Window");
                    return false;
                }

                window = sdlWindow;

                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

                glContext = SDL_GL_CreateContext(window);
                if (!glContext) {
                    RTB_ERROR("OpenGLRenderDevice: Failed to create OpenGL context: " + std::string(SDL_GetError()));
                    return false;
                }

                glewExperimental = GL_TRUE;
                const GLenum glewError = glewInit();
                if (glewError != GLEW_OK) {
                    RTB_ERROR("OpenGLRenderDevice: Failed to initialize GLEW: "
                              + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewError))));
                    SDL_GL_DeleteContext(glContext);
                    glContext = nullptr;
                    return false;
                }

                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glFrontFace(GL_CCW);

                SetVSync(vSync);
                initialized = true;
                RTB_INFO("OpenGLRenderDevice initialized (OpenGL 4.3 Core)");
                return true;
            }

            void OpenGLRenderDevice::Shutdown()
            {
                if (glContext) {
                    SDL_GL_DeleteContext(glContext);
                    glContext = nullptr;
                }
                window = nullptr;
                initialized = false;
            }

            void OpenGLRenderDevice::MakeCurrent()
            {
                if (window && glContext) {
                    SDL_GL_MakeCurrent(window, glContext);
                }
            }

            void OpenGLRenderDevice::Present()
            {
                if (window) {
                    SDL_GL_SwapWindow(window);
                }
            }

            void OpenGLRenderDevice::SetVSync(bool enabled)
            {
                SDL_GL_SetSwapInterval(enabled ? 1 : 0);
            }

            void OpenGLRenderDevice::SetViewport(int x, int y, int width, int height)
            {
                glViewport(x, y, width, height);
            }

            void OpenGLRenderDevice::SetClearColor(float r, float g, float b, float a)
            {
                glClearColor(r, g, b, a);
            }

            void OpenGLRenderDevice::Clear(ClearMask mask)
            {
                GLbitfield bits = 0;
                if ((mask & ClearMask::Color) != ClearMask::None) {
                    bits |= GL_COLOR_BUFFER_BIT;
                }
                if ((mask & ClearMask::Depth) != ClearMask::None) {
                    bits |= GL_DEPTH_BUFFER_BIT;
                }
                if (bits != 0) {
                    glClear(bits);
                }
            }

            void OpenGLRenderDevice::SetDepthTest(bool enabled)
            {
                if (enabled) {
                    glEnable(GL_DEPTH_TEST);
                }
                else {
                    glDisable(GL_DEPTH_TEST);
                }
            }

            void OpenGLRenderDevice::SetDepthFunc(DepthFunc func)
            {
                glDepthFunc(ToGLDepthFunc(func));
            }

            void OpenGLRenderDevice::SetDepthWrite(bool enabled)
            {
                glDepthMask(enabled ? GL_TRUE : GL_FALSE);
            }

            void OpenGLRenderDevice::SetCullFace(bool enabled)
            {
                if (enabled) {
                    glEnable(GL_CULL_FACE);
                }
                else {
                    glDisable(GL_CULL_FACE);
                }
            }

            void OpenGLRenderDevice::SetBlend(bool enabled)
            {
                if (enabled) {
                    glEnable(GL_BLEND);
                }
                else {
                    glDisable(GL_BLEND);
                }
            }

            void OpenGLRenderDevice::SetBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha)
            {
                glBlendFuncSeparate(static_cast<GLenum>(srcRGB), static_cast<GLenum>(dstRGB),
                                    static_cast<GLenum>(srcAlpha), static_cast<GLenum>(dstAlpha));
            }

            void OpenGLRenderDevice::SetColorMask(bool red, bool green, bool blue, bool alpha)
            {
                glColorMask(red ? GL_TRUE : GL_FALSE,
                            green ? GL_TRUE : GL_FALSE,
                            blue ? GL_TRUE : GL_FALSE,
                            alpha ? GL_TRUE : GL_FALSE);
            }

            GpuId OpenGLRenderDevice::CreateShaderProgram(const std::string& vertexSource,
                                                           const std::string& fragmentSource)
            {
                auto compile = [](GLenum type, const std::string& source) -> GLuint {
                    const GLuint shader = glCreateShader(type);
                    const char* src = source.c_str();
                    glShaderSource(shader, 1, &src, nullptr);
                    glCompileShader(shader);

                    GLint success = 0;
                    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                    if (!success) {
                        GLchar infoLog[512];
                        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
                        RTB_ERROR(std::string("Shader compilation failed: ") + infoLog);
                        glDeleteShader(shader);
                        return 0;
                    }
                    return shader;
                };

                const GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
                if (vertexShader == 0) {
                    return kInvalidGpuId;
                }

                const GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);
                if (fragmentShader == 0) {
                    glDeleteShader(vertexShader);
                    return kInvalidGpuId;
                }

                const GLuint program = glCreateProgram();
                glAttachShader(program, vertexShader);
                glAttachShader(program, fragmentShader);
                glLinkProgram(program);

                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);

                GLint success = 0;
                glGetProgramiv(program, GL_LINK_STATUS, &success);
                if (!success) {
                    GLchar infoLog[512];
                    glGetProgramInfoLog(program, 512, nullptr, infoLog);
                    RTB_ERROR(std::string("Shader linking failed: ") + infoLog);
                    glDeleteProgram(program);
                    return kInvalidGpuId;
                }

                BindUniformBlock(program, "LightingData", kLightingUBOBinding);
                BindUniformBlock(program, "CameraData", kCameraUBOBinding);
                BindUniformBlock(program, "BoneData", kBoneUBOBinding);

                return program;
            }

            void OpenGLRenderDevice::DestroyShaderProgram(GpuId program)
            {
                if (program != kInvalidGpuId) {
                    glDeleteProgram(program);
                }
            }

            void OpenGLRenderDevice::BindShaderProgram(GpuId program)
            {
                glUseProgram(program);
            }

            int OpenGLRenderDevice::GetUniformLocation(GpuId program, const char* name)
            {
                return glGetUniformLocation(program, name);
            }

            void OpenGLRenderDevice::SetUniformBool(int location, bool value)
            {
                glUniform1i(location, value ? 1 : 0);
            }

            void OpenGLRenderDevice::SetUniformInt(int location, int value)
            {
                glUniform1i(location, value);
            }

            void OpenGLRenderDevice::SetUniformFloat(int location, float value)
            {
                glUniform1f(location, value);
            }

            void OpenGLRenderDevice::SetUniformVec2(int location, float x, float y)
            {
                glUniform2f(location, x, y);
            }

            void OpenGLRenderDevice::SetUniformVec3(int location, float x, float y, float z)
            {
                glUniform3f(location, x, y, z);
            }

            void OpenGLRenderDevice::SetUniformVec4(int location, float x, float y, float z, float w)
            {
                glUniform4f(location, x, y, z, w);
            }

            void OpenGLRenderDevice::SetUniformMat4(int location, const float* matrix4x4)
            {
                glUniformMatrix4fv(location, 1, GL_FALSE, matrix4x4);
            }

            void OpenGLRenderDevice::BindUniformBlock(GpuId program, const char* blockName, unsigned int bindingPoint)
            {
                const GLuint blockIndex = glGetUniformBlockIndex(program, blockName);
                if (blockIndex != GL_INVALID_INDEX) {
                    glUniformBlockBinding(program, blockIndex, bindingPoint);
                }
            }

            GpuId OpenGLRenderDevice::CreateBuffer()
            {
                GLuint buffer = 0;
                glGenBuffers(1, &buffer);
                return buffer;
            }

            void OpenGLRenderDevice::DestroyBuffer(GpuId buffer)
            {
                if (buffer != kInvalidGpuId) {
                    const GLuint id = buffer;
                    glDeleteBuffers(1, &id);
                }
            }

            void OpenGLRenderDevice::SetUniformBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage)
            {
                glBindBuffer(GL_UNIFORM_BUFFER, buffer);
                glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, ToGLBufferUsage(usage));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            void OpenGLRenderDevice::UpdateUniformBufferData(GpuId buffer, const void* data, std::size_t size)
            {
                glBindBuffer(GL_UNIFORM_BUFFER, buffer);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(size), data);
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            void OpenGLRenderDevice::BindUniformBufferBase(GpuId buffer, unsigned int bindingPoint)
            {
                glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, buffer);
            }

            GpuId OpenGLRenderDevice::CreateTexture2D()
            {
                GLuint texture = 0;
                glGenTextures(1, &texture);
                return texture;
            }

            void OpenGLRenderDevice::DestroyTexture(GpuId texture)
            {
                if (texture != kInvalidGpuId) {
                    const GLuint id = texture;
                    glDeleteTextures(1, &id);
                }
            }

            void OpenGLRenderDevice::ResolveTextureFormat(TextureFormat format, unsigned int& internalFormat,
                                                          unsigned int& pixelFormat, unsigned int& pixelType)
            {
                pixelType = GL_UNSIGNED_BYTE;
                switch (format) {
                case TextureFormat::R8:
                    internalFormat = GL_RED;
                    pixelFormat = GL_RED;
                    break;
                case TextureFormat::RGB8:
                    internalFormat = GL_RGB;
                    pixelFormat = GL_RGB;
                    break;
                case TextureFormat::RGBA8:
                    internalFormat = GL_RGBA8;
                    pixelFormat = GL_RGBA;
                    break;
                case TextureFormat::SRGB8:
                    internalFormat = GL_SRGB8;
                    pixelFormat = GL_RGB;
                    break;
                case TextureFormat::SRGBA8:
                    internalFormat = GL_SRGB8_ALPHA8;
                    pixelFormat = GL_RGBA;
                    break;
                case TextureFormat::Depth24:
                    internalFormat = GL_DEPTH_COMPONENT24;
                    pixelFormat = GL_DEPTH_COMPONENT;
                    pixelType = GL_FLOAT;
                    break;
                case TextureFormat::Depth32F:
                    internalFormat = GL_DEPTH_COMPONENT32F;
                    pixelFormat = GL_DEPTH_COMPONENT;
                    pixelType = GL_FLOAT;
                    break;
                default:
                    internalFormat = GL_RGBA8;
                    pixelFormat = GL_RGBA;
                    break;
                }
            }

            void OpenGLRenderDevice::SetTexture2DData(GpuId texture, TextureFormat format, int width, int height,
                                                     const void* pixels, bool generateMipmaps)
            {
                unsigned int internalFormat = 0;
                unsigned int pixelFormat = 0;
                unsigned int pixelType = 0;
                ResolveTextureFormat(format, internalFormat, pixelFormat, pixelType);

                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0,
                             pixelFormat, pixelType, pixels);
                if (generateMipmaps && pixels != nullptr
                    && format != TextureFormat::Depth24 && format != TextureFormat::Depth32F) {
                    glGenerateMipmap(GL_TEXTURE_2D);
                }
            }

            void OpenGLRenderDevice::SetTexture2DFilter(GpuId texture, TextureFilter minFilter, TextureFilter magFilter)
            {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(ToGLFilter(minFilter)));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(ToGLFilter(magFilter)));
            }

            void OpenGLRenderDevice::SetTexture2DWrap(GpuId texture, TextureWrap wrapS, TextureWrap wrapT)
            {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(ToGLWrap(wrapS)));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(ToGLWrap(wrapT)));
            }

            void OpenGLRenderDevice::SetTexture2DDepthShadowParams(GpuId texture)
            {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            void OpenGLRenderDevice::BindTexture2D(GpuId texture, unsigned int slot)
            {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, texture);
            }

            void OpenGLRenderDevice::UnbindTexture2D()
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            GpuId OpenGLRenderDevice::CreateCubemap()
            {
                GLuint texture = 0;
                glGenTextures(1, &texture);
                return texture;
            }

            void OpenGLRenderDevice::SetCubemapFace(GpuId cubemap, int faceIndex, TextureFormat format,
                                                   int width, int height, const void* pixels)
            {
                unsigned int internalFormat = 0;
                unsigned int pixelFormat = 0;
                unsigned int pixelType = 0;
                ResolveTextureFormat(format, internalFormat, pixelFormat, pixelType);

                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, static_cast<GLint>(internalFormat),
                             width, height, 0, pixelFormat, pixelType, pixels);
            }

            void OpenGLRenderDevice::SetCubemapFilterWrap(GpuId cubemap)
            {
                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }

            void OpenGLRenderDevice::BindCubemap(GpuId cubemap, unsigned int slot)
            {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
            }

            GpuId OpenGLRenderDevice::CreateFramebuffer()
            {
                GLuint fbo = 0;
                glGenFramebuffers(1, &fbo);
                return fbo;
            }

            void OpenGLRenderDevice::DestroyFramebuffer(GpuId framebuffer)
            {
                if (framebuffer != kInvalidGpuId) {
                    const GLuint id = framebuffer;
                    glDeleteFramebuffers(1, &id);
                }
            }

            void OpenGLRenderDevice::BindFramebuffer(GpuId framebuffer)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            }

            void OpenGLRenderDevice::UnbindFramebuffer()
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            void OpenGLRenderDevice::AttachFramebufferColorTexture(GpuId /*framebuffer*/, GpuId texture)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            }

            void OpenGLRenderDevice::AttachFramebufferDepthTexture(GpuId /*framebuffer*/, GpuId texture)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
            }

            void OpenGLRenderDevice::SetFramebufferDrawReadNone()
            {
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }

            bool OpenGLRenderDevice::IsFramebufferComplete() const
            {
                return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            }

            GpuId OpenGLRenderDevice::CreateColorTextureForFramebuffer(int width, int height)
            {
                const GpuId texture = CreateTexture2D();
                SetTexture2DData(texture, TextureFormat::RGBA8, width, height, nullptr, false);
                SetTexture2DFilter(texture, TextureFilter::Linear, TextureFilter::Linear);
                SetTexture2DWrap(texture, TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
                return texture;
            }

            GpuId OpenGLRenderDevice::CreateDepthTextureForFramebuffer(int width, int height)
            {
                const GpuId texture = CreateTexture2D();
                SetTexture2DData(texture, TextureFormat::Depth24, width, height, nullptr, false);
                SetTexture2DFilter(texture, TextureFilter::Nearest, TextureFilter::Nearest);
                return texture;
            }

            GpuId OpenGLRenderDevice::CreateVertexArray()
            {
                GLuint vao = 0;
                glGenVertexArrays(1, &vao);
                return vao;
            }

            void OpenGLRenderDevice::DestroyVertexArray(GpuId vao)
            {
                if (vao != kInvalidGpuId) {
                    const GLuint id = vao;
                    glDeleteVertexArrays(1, &id);
                }
            }

            void OpenGLRenderDevice::BindVertexArray(GpuId vao)
            {
                glBindVertexArray(vao);
            }

            void OpenGLRenderDevice::UnbindVertexArray()
            {
                glBindVertexArray(0);
            }

            void OpenGLRenderDevice::SetArrayBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage)
            {
                glBindBuffer(GL_ARRAY_BUFFER, buffer);
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), data, ToGLBufferUsage(usage));
            }

            void OpenGLRenderDevice::SetElementBufferData(GpuId buffer, const void* data, std::size_t size, BufferUsage usage)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), data, ToGLBufferUsage(usage));
            }

            void OpenGLRenderDevice::BindArrayBuffer(GpuId buffer)
            {
                glBindBuffer(GL_ARRAY_BUFFER, buffer);
            }

            void OpenGLRenderDevice::BindElementBuffer(GpuId buffer)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            }

            void OpenGLRenderDevice::EnableVertexAttribFloat(unsigned int location, int components, int stride, std::size_t offset)
            {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, stride,
                                      reinterpret_cast<const void*>(offset));
            }

            void OpenGLRenderDevice::EnableVertexAttribInt(unsigned int location, int components, int stride, std::size_t offset)
            {
                glEnableVertexAttribArray(location);
                glVertexAttribIPointer(location, components, GL_INT, stride,
                                       reinterpret_cast<const void*>(offset));
            }

            void OpenGLRenderDevice::SetVertexAttribDivisor(unsigned int location, unsigned int divisor)
            {
                glVertexAttribDivisor(location, divisor);
            }

            void OpenGLRenderDevice::DrawIndexed(PrimitiveTopology topology, int indexCount, IndexType indexType)
            {
                glDrawElements(ToGLTopology(topology), indexCount, ToGLIndexType(indexType), nullptr);
            }

            void OpenGLRenderDevice::DrawIndexedInstanced(PrimitiveTopology topology, int indexCount, IndexType indexType, int instanceCount)
            {
                glDrawElementsInstanced(ToGLTopology(topology), indexCount, ToGLIndexType(indexType), nullptr, instanceCount);
            }

            void OpenGLRenderDevice::DrawArrays(PrimitiveTopology topology, int first, int count)
            {
                glDrawArrays(ToGLTopology(topology), first, count);
            }

            void OpenGLRenderDevice::DrawArraysInstanced(PrimitiveTopology topology, int first, int count, int instanceCount)
            {
                glDrawArraysInstanced(ToGLTopology(topology), first, count, instanceCount);
            }

            bool OpenGLRenderDevice::InitializeImGuiBackend(SDL_Window* sdlWindow)
            {
                if (imguiBackendInitialized) return true;
                if (!ImGui_ImplSDL2_InitForOpenGL(sdlWindow, glContext)) return false;
                if (!ImGui_ImplOpenGL3_Init("#version 330")) {
                    ImGui_ImplSDL2_Shutdown();
                    return false;
                }
                imguiBackendInitialized = true;
                return true;
            }

            void OpenGLRenderDevice::ShutdownImGuiBackend()
            {
                if (!imguiBackendInitialized) return;
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplSDL2_Shutdown();
                imguiBackendInitialized = false;
            }

            void OpenGLRenderDevice::BeginImGuiFrame()
            {
                if (!imguiBackendInitialized) return;
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL2_NewFrame();
            }

            void OpenGLRenderDevice::QueueImGuiDrawData(ImDrawData* drawData)
            {
                // OpenGL has no deferred pass system: render immediately.
                if (imguiBackendInitialized && drawData) {
                    ImGui_ImplOpenGL3_RenderDrawData(drawData);
                }
            }

            std::uintptr_t OpenGLRenderDevice::GetNativeTextureIdForImGui(GpuId texture) const
            {
                return texture;
            }

            unsigned int OpenGLRenderDevice::ToGLDepthFunc(DepthFunc func)
            {
                switch (func) {
                case DepthFunc::Less: return GL_LESS;
                case DepthFunc::LEqual: return GL_LEQUAL;
                case DepthFunc::Greater: return GL_GREATER;
                case DepthFunc::GEqual: return GL_GEQUAL;
                case DepthFunc::Always: return GL_ALWAYS;
                case DepthFunc::Never: return GL_NEVER;
                case DepthFunc::Equal: return GL_EQUAL;
                case DepthFunc::NotEqual: return GL_NOTEQUAL;
                default: return GL_LESS;
                }
            }

            unsigned int OpenGLRenderDevice::ToGLBufferUsage(BufferUsage usage)
            {
                return usage == BufferUsage::Dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
            }

            unsigned int OpenGLRenderDevice::ToGLTopology(PrimitiveTopology topology)
            {
                switch (topology) {
                case PrimitiveTopology::Triangles: return GL_TRIANGLES;
                case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
                case PrimitiveTopology::Lines: return GL_LINES;
                case PrimitiveTopology::LineStrip: return GL_LINE_STRIP;
                case PrimitiveTopology::Points: return GL_POINTS;
                default: return GL_TRIANGLES;
                }
            }

            unsigned int OpenGLRenderDevice::ToGLIndexType(IndexType type)
            {
                return type == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            }

            unsigned int OpenGLRenderDevice::ToGLFilter(TextureFilter filter)
            {
                switch (filter) {
                case TextureFilter::Nearest: return GL_NEAREST;
                case TextureFilter::Linear: return GL_LINEAR;
                case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
                default: return GL_LINEAR;
                }
            }

            unsigned int OpenGLRenderDevice::ToGLWrap(TextureWrap wrap)
            {
                switch (wrap) {
                case TextureWrap::Repeat: return GL_REPEAT;
                case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
                case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
                case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
                default: return GL_REPEAT;
                }
            }

        }
    }
}
