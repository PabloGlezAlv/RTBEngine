#include "VolumetricFogPass.h"

#include "Camera.h"
#include "CameraUBO.h"
#include "FogUniforms.h"
#include "Shader.h"
#include "Lighting/DirectionalLight.h"
#include "Lighting/LightingProjectSettings.h"
#include "Lighting/LightingUBO.h"
#include "RHI/RenderDevice.h"
#include "RHI/GraphicsAPI.h"
#include "ShadowMap.h"

namespace RTBEngine {
    namespace Rendering {

        namespace {
            // Fullscreen triangle in clip space (covers NDC with one triangle).
            constexpr float kFullscreenTriangle[] = {
                // pos.xy, uv.xy
                -1.0f, -1.0f, 0.0f, 0.0f,
                 3.0f, -1.0f, 2.0f, 0.0f,
                -1.0f,  3.0f, 0.0f, 2.0f,
            };

            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        VolumetricFogPass& VolumetricFogPass::GetInstance()
        {
            static VolumetricFogPass instance;
            return instance;
        }

        VolumetricFogPass::~VolumetricFogPass()
        {
            Shutdown();
        }

        bool VolumetricFogPass::Initialize(Shader* fogShader)
        {
            if (!fogShader) {
                return false;
            }

            shader = fogShader;
            CreateFullscreenTriangle();
            return IsReady();
        }

        void VolumetricFogPass::Shutdown()
        {
            DestroyFullscreenTriangle();
            shader = nullptr;
        }

        void VolumetricFogPass::CreateFullscreenTriangle()
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

        void VolumetricFogPass::DestroyFullscreenTriangle()
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

        void VolumetricFogPass::Render(Camera* camera,
                                       RHI::GpuId sceneDepthTexture,
                                       DirectionalLight* shadowLight,
                                       const Math::Matrix4& lightSpaceMatrix)
        {
            const auto& settings = LightingProjectSettings::Get();
            if (!settings.volumetricFogEnabled || !settings.fogEnabled || !IsReady() || !camera) {
                return;
            }
            if (sceneDepthTexture == RHI::kInvalidGpuId) {
                return;
            }

            auto& device = Device();

            device.SetDepthTest(false);
            device.SetDepthWrite(false);
            device.SetCullFace(false);
            device.SetBlend(true);
            device.SetBlendFuncSeparate(
                RHI::BlendFactor::One,
                RHI::BlendFactor::SrcAlpha,
                RHI::BlendFactor::One,
                RHI::BlendFactor::SrcAlpha);

            shader->Bind();
            CameraUBO::GetInstance().Bind();
            LightingUBO::GetInstance().Bind();
            FogUniforms::Apply(shader);
            FogUniforms::ApplyCameraPlanes(shader, camera);

            const Math::Matrix4 invViewProj = camera->GetViewProjectionMatrix().Inverse();
            shader->SetMatrix4("uViewProjection", invViewProj);
            shader->SetMatrix4("uLightSpaceMatrix", lightSpaceMatrix);

            const bool hasShadows = shadowLight
                && shadowLight->GetCastShadows()
                && settings.shadowsEnabled
                && shadowLight->GetShadowMap();
            shader->SetBool("uHasShadows", hasShadows);

            device.BindTexture2D(sceneDepthTexture, 0);
            shader->SetInt("uSceneDepth", 0);

            if (hasShadows) {
                shadowLight->GetShadowMap()->BindForReading(1);
                shader->SetInt("uShadowMap", 1);
                shader->SetFloat("uShadowBias", shadowLight->GetShadowBias());
            }

            device.BindVertexArray(vao);
            device.DrawArrays(RHI::PrimitiveTopology::Triangles, 0, 3);
            device.UnbindVertexArray();

            device.UnbindTexture2D();
            shader->Unbind();

            device.SetBlend(false);
            device.SetDepthTest(true);
            device.SetDepthWrite(true);
            device.SetCullFace(true);
            device.SetDepthFunc(RHI::DepthFunc::Less);
        }

    }
}
