#include "FogUniforms.h"

#include "Camera.h"
#include "Shader.h"
#include "Lighting/LightingProjectSettings.h"
#include "RHI/RenderDevice.h"
#include "RHI/GraphicsAPI.h"

namespace RTBEngine {
    namespace Rendering {

        void FogUniforms::Apply(Shader* shader)
        {
            if (!shader) {
                return;
            }

            const auto& fog = LightingProjectSettings::Get();
            shader->SetBool("uFogEnabled", fog.fogEnabled);
            shader->SetVector3("uFogColor", fog.fogColor);
            shader->SetFloat("uFogDensity", fog.fogDensity);
            shader->SetFloat("uFogHeight", fog.fogHeight);
            shader->SetFloat("uFogHeightFalloff", fog.fogHeightFalloff);
            shader->SetFloat("uFogStart", fog.fogStart);
            shader->SetFloat("uFogEnd", fog.fogEnd);
            shader->SetBool("uVolumetricFogEnabled", fog.volumetricFogEnabled);
            shader->SetFloat("uVolumetricIntensity", fog.volumetricIntensity);
            shader->SetFloat("uVolumetricAnisotropy", fog.volumetricAnisotropy);
            shader->SetInt("uVolumetricSamples", fog.volumetricSamples);
            shader->SetFloat("uVolumetricMaxLuminance", fog.volumetricMaxLuminance);

            const bool depthZeroToOne =
                RHI::RenderDevice::HasDevice()
                && RHI::RenderDevice::Get().GetAPI() == RHI::GraphicsAPI::Vulkan;
            shader->SetBool("uDepthZeroToOne", depthZeroToOne);
        }

        void FogUniforms::ApplyCameraPlanes(Shader* shader, const Camera* camera)
        {
            if (!shader || !camera) {
                return;
            }

            shader->SetFloat("uCameraNear", camera->GetNearPlane());
            shader->SetFloat("uCameraFar", camera->GetFarPlane());
        }

    }
}
