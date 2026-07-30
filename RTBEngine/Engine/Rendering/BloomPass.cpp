#include "BloomPass.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "Shader.h"
#include "RHI/RenderDevice.h"

#include <algorithm>

namespace RTBEngine {
    namespace Rendering {

        namespace {
            constexpr float kFullscreenTriangle[] = {
                -1.0f, -1.0f, 0.0f, 0.0f,
                 3.0f, -1.0f, 2.0f, 0.0f,
                -1.0f,  3.0f, 0.0f, 2.0f,
            };

            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }

            void DestroyTextureAndFramebuffer(RHI::GpuId& fbo, RHI::GpuId& texture)
            {
                if (!RHI::RenderDevice::HasDevice()) {
                    fbo = RHI::kInvalidGpuId;
                    texture = RHI::kInvalidGpuId;
                    return;
                }

                auto& device = Device();
                if (fbo != RHI::kInvalidGpuId) {
                    device.DestroyFramebuffer(fbo);
                    fbo = RHI::kInvalidGpuId;
                }
                if (texture != RHI::kInvalidGpuId) {
                    device.DestroyTexture(texture);
                    texture = RHI::kInvalidGpuId;
                }
            }

            bool CreateColorTarget(RHI::GpuId& fbo, RHI::GpuId& texture, int width, int height)
            {
                auto& device = Device();
                fbo = device.CreateFramebuffer();
                texture = device.CreateColorTextureForFramebuffer(width, height);
                if (fbo == RHI::kInvalidGpuId || texture == RHI::kInvalidGpuId) {
                    return false;
                }

                device.BindFramebuffer(fbo);
                device.AttachFramebufferColorTexture(fbo, texture);
                const bool complete = device.IsFramebufferComplete();
                device.UnbindFramebuffer();
                return complete;
            }
        }

        BloomPass& BloomPass::GetInstance()
        {
            static BloomPass instance;
            return instance;
        }

        BloomPass::~BloomPass()
        {
            Shutdown();
        }

        bool BloomPass::Initialize()
        {
            if (!RHI::RenderDevice::HasDevice()) {
                return false;
            }

            auto& resources = Core::ResourceManager::GetInstance();

            extractShader = resources.LoadShader(
                "bloom_extract",
                "Default/Shaders/fullscreen.vert",
                "Default/Shaders/bloom_extract.frag");
            blurShader = resources.LoadShader(
                "bloom_blur",
                "Default/Shaders/fullscreen.vert",
                "Default/Shaders/bloom_blur.frag");
            compositeShader = resources.LoadShader(
                "bloom_composite",
                "Default/Shaders/fullscreen.vert",
                "Default/Shaders/bloom_composite.frag");
            copyShader = resources.LoadShader(
                "bloom_copy",
                "Default/Shaders/fullscreen.vert",
                "Default/Shaders/bloom_copy.frag");

            if (!extractShader || !blurShader || !compositeShader || !copyShader) {
                RTB_WARN("BloomPass: one or more bloom shaders failed to load (bloom disabled)");
                Shutdown();
                return false;
            }

            CreateFullscreenTriangle();
            return IsReady();
        }

        void BloomPass::Shutdown()
        {
            DestroyRenderTargets();
            DestroyFullscreenTriangle();

            extractShader = nullptr;
            blurShader = nullptr;
            compositeShader = nullptr;
            copyShader = nullptr;
        }

        bool BloomPass::IsReady() const
        {
            return extractShader && blurShader && compositeShader && copyShader
                && vao != RHI::kInvalidGpuId;
        }

        void BloomPass::CreateFullscreenTriangle()
        {
            if (!RHI::RenderDevice::HasDevice()) {
                return;
            }

            DestroyFullscreenTriangle();

            auto& device = Device();
            vao = device.CreateVertexArray();
            vbo = device.CreateBuffer();
            device.BindVertexArray(vao);
            device.SetArrayBufferData(vbo, kFullscreenTriangle, sizeof(kFullscreenTriangle), RHI::BufferUsage::Static);
            device.EnableVertexAttribFloat(0, 2, static_cast<int>(4 * sizeof(float)), 0);
            device.EnableVertexAttribFloat(1, 2, static_cast<int>(4 * sizeof(float)), 2 * sizeof(float));
            device.UnbindVertexArray();
        }

        void BloomPass::DestroyFullscreenTriangle()
        {
            if (!RHI::RenderDevice::HasDevice()) {
                vao = RHI::kInvalidGpuId;
                vbo = RHI::kInvalidGpuId;
                return;
            }

            auto& device = Device();
            if (vao != RHI::kInvalidGpuId) {
                device.DestroyVertexArray(vao);
                vao = RHI::kInvalidGpuId;
            }
            if (vbo != RHI::kInvalidGpuId) {
                device.DestroyBuffer(vbo);
                vbo = RHI::kInvalidGpuId;
            }
        }

        void BloomPass::DestroyRenderTargets()
        {
            DestroyTextureAndFramebuffer(brightFbo, brightTexture);
            DestroyTextureAndFramebuffer(pingFbo, pingTexture);
            DestroyTextureAndFramebuffer(pongFbo, pongTexture);
            DestroyTextureAndFramebuffer(scratchFbo, scratchTexture);

            if (outputFbo != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyFramebuffer(outputFbo);
            }
            outputFbo = RHI::kInvalidGpuId;
            outputColorTexture = RHI::kInvalidGpuId;

            targetWidth = 0;
            targetHeight = 0;
            bloomWidth = 0;
            bloomHeight = 0;
        }

        bool BloomPass::EnsureRenderTargets(int width, int height)
        {
            if (width <= 0 || height <= 0) {
                return false;
            }

            const int halfWidth = std::max(1, width / 2);
            const int halfHeight = std::max(1, height / 2);
            if (targetWidth == width && targetHeight == height
                && brightTexture != RHI::kInvalidGpuId
                && scratchTexture != RHI::kInvalidGpuId) {
                return true;
            }

            DestroyRenderTargets();
            targetWidth = width;
            targetHeight = height;
            bloomWidth = halfWidth;
            bloomHeight = halfHeight;

            if (!CreateColorTarget(brightFbo, brightTexture, bloomWidth, bloomHeight)) {
                DestroyRenderTargets();
                return false;
            }
            if (!CreateColorTarget(pingFbo, pingTexture, bloomWidth, bloomHeight)) {
                DestroyRenderTargets();
                return false;
            }
            if (!CreateColorTarget(pongFbo, pongTexture, bloomWidth, bloomHeight)) {
                DestroyRenderTargets();
                return false;
            }
            if (!CreateColorTarget(scratchFbo, scratchTexture, width, height)) {
                DestroyRenderTargets();
                return false;
            }

            return true;
        }

        bool BloomPass::EnsureOutputFramebuffer(RHI::GpuId sceneColorTexture, int width, int height)
        {
            if (sceneColorTexture == RHI::kInvalidGpuId || width <= 0 || height <= 0) {
                return false;
            }

            if (outputFbo != RHI::kInvalidGpuId
                && outputColorTexture == sceneColorTexture
                && targetWidth == width
                && targetHeight == height) {
                return true;
            }

            if (outputFbo != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyFramebuffer(outputFbo);
                outputFbo = RHI::kInvalidGpuId;
            }

            outputColorTexture = sceneColorTexture;
            auto& device = Device();
            outputFbo = device.CreateFramebuffer();
            if (outputFbo == RHI::kInvalidGpuId) {
                return false;
            }

            device.BindFramebuffer(outputFbo);
            device.AttachFramebufferColorTexture(outputFbo, sceneColorTexture);
            const bool complete = device.IsFramebufferComplete();
            device.UnbindFramebuffer();
            if (!complete) {
                device.DestroyFramebuffer(outputFbo);
                outputFbo = RHI::kInvalidGpuId;
                outputColorTexture = RHI::kInvalidGpuId;
                return false;
            }

            return true;
        }

        void BloomPass::DrawFullscreen(Shader* shader)
        {
            if (!shader) {
                return;
            }

            auto& device = Device();
            shader->Bind();
            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            device.UnbindVertexArray();
            shader->Unbind();
        }

        void BloomPass::Render(RHI::GpuId sceneColorTexture, int width, int height)
        {
            if (!IsReady()
                || sceneColorTexture == RHI::kInvalidGpuId
                || width <= 0
                || height <= 0) {
                return;
            }

            if (!EnsureRenderTargets(width, height)
                || !EnsureOutputFramebuffer(sceneColorTexture, width, height)) {
                return;
            }

            auto& device = Device();

            device.SetDepthTest(false);
            device.SetDepthWrite(false);
            device.SetCullFace(false);
            device.SetBlend(false);

            // 1) Extract bright regions.
            device.BindFramebuffer(brightFbo);
            device.SetViewport(0, 0, bloomWidth, bloomHeight);
            extractShader->Bind();
            device.BindTexture2D(sceneColorTexture, 0);
            extractShader->SetInt("uSceneColor", 0);
            extractShader->SetFloat("uThreshold", threshold);
            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            extractShader->Unbind();

            // 2) Separable blur (two iterations).
            RHI::GpuId sourceTexture = brightTexture;
            const float texelX = 1.0f / static_cast<float>(bloomWidth);
            const float texelY = 1.0f / static_cast<float>(bloomHeight);

            for (int pass = 0; pass < 2; ++pass) {
                device.BindFramebuffer(pingFbo);
                blurShader->Bind();
                device.BindTexture2D(sourceTexture, 0);
                blurShader->SetInt("uInput", 0);
                blurShader->SetFloat("uTexelSizeX", texelX);
                blurShader->SetFloat("uTexelSizeY", 0.0f);
                device.BindVertexArray(vao);
                device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);

                device.BindFramebuffer(pongFbo);
                device.BindTexture2D(pingTexture, 0);
                blurShader->SetInt("uInput", 0);
                blurShader->SetFloat("uTexelSizeX", 0.0f);
                blurShader->SetFloat("uTexelSizeY", texelY);
                device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
                blurShader->Unbind();

                sourceTexture = pongTexture;
            }

            // 3) Tonemap(scene + bloom) into scratch (avoids read/write feedback on scene color).
            device.BindFramebuffer(scratchFbo);
            device.SetViewport(0, 0, width, height);
            compositeShader->Bind();
            device.BindTexture2D(sceneColorTexture, 0);
            device.BindTexture2D(pongTexture, 1);
            compositeShader->SetInt("uSceneColor", 0);
            compositeShader->SetInt("uBloom", 1);
            compositeShader->SetFloat("uIntensity", intensity);
            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            compositeShader->Unbind();

            // 4) Copy scratch back to the scene color attachment.
            device.BindFramebuffer(outputFbo);
            copyShader->Bind();
            device.BindTexture2D(scratchTexture, 0);
            copyShader->SetInt("uSource", 0);
            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            copyShader->Unbind();
            device.UnbindVertexArray();

            device.BindTexture2D(RHI::kInvalidGpuId, 0);
            device.BindTexture2D(RHI::kInvalidGpuId, 1);
            device.UnbindFramebuffer();

            device.SetDepthTest(true);
            device.SetDepthWrite(true);
            device.SetCullFace(true);
        }

        bool BloomPass::RenderIfReady(RHI::GpuId sceneColorTexture, int width, int height)
        {
            if (!IsReady()) {
                return false;
            }

            Render(sceneColorTexture, width, height);
            return true;
        }

    }
}
