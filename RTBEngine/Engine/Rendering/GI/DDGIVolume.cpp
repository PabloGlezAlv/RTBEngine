#include "DDGIVolume.h"
#include "../RHI/RenderDevice.h"
#include "../Lighting/LightingUBO.h"
#include "../Lighting/LightingProjectSettings.h"
#include "../../Core/Logger.h"
#include <cstring>
#include <vector>

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            DDGIVolume::DDGIVolume(const DDGISettings& s) : settings(s) {}

            DDGIVolume::~DDGIVolume()
            {
                if (RHI::RenderDevice::HasDevice()) {
                    DestroyGpuResources(RHI::RenderDevice::Get());
                }
            }

            void DDGIVolume::SetSettings(const DDGISettings& s)
            {
                if (s.gridX != settings.gridX || s.gridY != settings.gridY || s.gridZ != settings.gridZ
                    || DDGIAtlasWidth(s) != DDGIAtlasWidth(settings)
                    || DDGIAtlasHeight(s) != DDGIAtlasHeight(settings)) {
                    if (RHI::RenderDevice::HasDevice()) {
                        DestroyGpuResources(RHI::RenderDevice::Get());
                    }
                }
                settings = s;
            }

            void DDGIVolume::EnsureGpuResources(RHI::IRenderDevice& device)
            {
                const int atlasW = DDGIAtlasWidth(settings);
                const int atlasH = DDGIAtlasHeight(settings);
                if (atlasW <= 0 || atlasH <= 0) {
                    return;
                }

                if (gpuReady) {
                    if (allocatedAtlasW == atlasW && allocatedAtlasH == atlasH) {
                        return;
                    }
                    DestroyGpuResources(device);
                }

                irradianceAtlas = device.CreateStorageImage2D(atlasW, atlasH, RHI::TextureFormat::RGBA16F);
                distanceAtlas = device.CreateStorageImage2D(atlasW, atlasH, RHI::TextureFormat::RGBA16F);
                ubo = device.CreateBuffer();

                if (irradianceAtlas == RHI::kInvalidGpuId || distanceAtlas == RHI::kInvalidGpuId) {
                    RTB_WARN("DDGIVolume: failed to create GPU resources");
                    DestroyGpuResources(device);
                    return;
                }

                // Irradiance: sqrt-encoded mild ambient (~sqrt(0.08)).
                device.ClearStorageImage2D(irradianceAtlas, 0.28f, 0.28f, 0.28f, 1.0f);
                // Distance: large mean/mean^2 so Chebyshev starts fully visible (RTXGI).
                device.ClearStorageImage2D(distanceAtlas, 50.0f, 2500.0f, 0.0f, 1.0f);

                allocatedAtlasW = atlasW;
                allocatedAtlasH = atlasH;
                gpuReady = true;
                UploadUBO(device);
            }

            void DDGIVolume::UploadUBO(RHI::IRenderDevice& device)
            {
                if (ubo == RHI::kInvalidGpuId) {
                    ubo = device.CreateBuffer();
                }

                DDGIUBOData data{};
                data.origin[0] = settings.origin.x;
                data.origin[1] = settings.origin.y;
                data.origin[2] = settings.origin.z;
                if (settings.gridX > 0) data.spacing[0] = settings.extent.x / static_cast<float>(settings.gridX);
                if (settings.gridY > 0) data.spacing[1] = settings.extent.y / static_cast<float>(settings.gridY);
                if (settings.gridZ > 0) data.spacing[2] = settings.extent.z / static_cast<float>(settings.gridZ);
                data.gridDims[0] = settings.gridX;
                data.gridDims[1] = settings.gridY;
                data.gridDims[2] = settings.gridZ;
                data.enabled = settings.enabled ? 1 : 0;
                data.hysteresis = settings.hysteresis;
                data.normalBias = settings.normalBias;
                data.viewBias = settings.viewBias;
                data.probeRadius = settings.probeRadius;

                const auto& lighting = LightingProjectSettings::Get();
                data.ambientColor[0] = lighting.ambientColor.x;
                data.ambientColor[1] = lighting.ambientColor.y;
                data.ambientColor[2] = lighting.ambientColor.z;
                data.ambientIntensity = lighting.ambientIntensity;
                data.ddgiIntensity = lighting.ddgiIntensity;

                device.SetUniformBufferData(ubo, &data, sizeof(data), RHI::BufferUsage::Dynamic);
            }

            void DDGIVolume::BindForSampling(RHI::IRenderDevice& device)
            {
                if (!gpuReady) {
                    return;
                }
                device.BindUniformBufferBase(ubo, RHI::kDDGIUBOBinding);
                device.BindTexture2D(irradianceAtlas, RHI::kDDGIIrradianceBinding);
                device.BindTexture2D(distanceAtlas, RHI::kDDGIDistanceBinding);
            }

            void DDGIVolume::DestroyGpuResources(RHI::IRenderDevice& device)
            {
                if (irradianceAtlas != RHI::kInvalidGpuId) {
                    device.DestroyTexture(irradianceAtlas);
                    irradianceAtlas = RHI::kInvalidGpuId;
                }
                if (distanceAtlas != RHI::kInvalidGpuId) {
                    device.DestroyTexture(distanceAtlas);
                    distanceAtlas = RHI::kInvalidGpuId;
                }
                if (ubo != RHI::kInvalidGpuId) {
                    device.DestroyBuffer(ubo);
                    ubo = RHI::kInvalidGpuId;
                }
                allocatedAtlasW = 0;
                allocatedAtlasH = 0;
                gpuReady = false;
            }

        }
    }
}
