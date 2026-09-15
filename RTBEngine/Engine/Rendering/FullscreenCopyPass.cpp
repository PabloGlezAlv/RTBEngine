#include "FullscreenCopyPass.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "Shader.h"
#include "RHI/RenderDevice.h"

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
        }

        FullscreenCopyPass& FullscreenCopyPass::GetInstance()
        {
            static FullscreenCopyPass instance;
            return instance;
        }

        FullscreenCopyPass::~FullscreenCopyPass()
        {
            Shutdown();
        }

        bool FullscreenCopyPass::Initialize()
        {
            if (!RHI::RenderDevice::HasDevice()) {
                return false;
            }

            shader = Core::ResourceManager::GetInstance().LoadShader(
                "fullscreen_copy",
                "Default/Shaders/fullscreen.vert",
                "Default/Shaders/bloom_copy.frag");
            if (!shader) {
                RTB_WARN("FullscreenCopyPass: copy shader failed to load");
                Shutdown();
                return false;
            }

            CreateFullscreenTriangle();
            return IsReady();
        }

        void FullscreenCopyPass::Shutdown()
        {
            DestroyFullscreenTriangle();
            shader = nullptr;
        }

        bool FullscreenCopyPass::IsReady() const
        {
            return shader && vao != RHI::kInvalidGpuId;
        }

        void FullscreenCopyPass::CreateFullscreenTriangle()
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

        void FullscreenCopyPass::DestroyFullscreenTriangle()
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

        void FullscreenCopyPass::CopyToBoundTarget(RHI::GpuId sourceTexture)
        {
            if (!IsReady() || sourceTexture == RHI::kInvalidGpuId) {
                return;
            }

            auto& device = Device();
            device.SetDepthTest(false);
            device.SetDepthWrite(false);
            device.SetCullFace(false);
            device.SetBlend(false);

            shader->Bind();
            device.BindTexture2D(sourceTexture, 0);
            shader->SetInt("uSource", 0);
            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            shader->Unbind();
            device.UnbindVertexArray();
            device.BindTexture2D(RHI::kInvalidGpuId, 0);

            device.SetDepthTest(true);
            device.SetDepthWrite(true);
            device.SetCullFace(true);
        }

    }
}
