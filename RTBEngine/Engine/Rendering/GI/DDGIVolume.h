#pragma once

#include "../../RTBEngineAPI.h"
#include "GiTypes.h"
#include "../RHI/RenderTypes.h"
#include "../RHI/IRenderDevice.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

#pragma warning(push)
#pragma warning(disable: 4251)
            class RTB_API DDGIVolume {
            public:
                explicit DDGIVolume(const DDGISettings& settings = {});
                ~DDGIVolume();

                DDGIVolume(const DDGIVolume&) = delete;
                DDGIVolume& operator=(const DDGIVolume&) = delete;

                void SetSettings(const DDGISettings& settings);
                const DDGISettings& GetSettings() const { return settings; }

                bool IsGpuReady() const { return gpuReady; }
                RHI::GpuId GetIrradianceAtlas() const { return irradianceAtlas; }
                RHI::GpuId GetDistanceAtlas() const { return distanceAtlas; }
                RHI::GpuId GetUBO() const { return ubo; }

                void EnsureGpuResources(RHI::IRenderDevice& device);
                void UploadUBO(RHI::IRenderDevice& device);
                void BindForSampling(RHI::IRenderDevice& device);

            private:
                void DestroyGpuResources(RHI::IRenderDevice& device);

                DDGISettings settings{};
                RHI::GpuId irradianceAtlas = RHI::kInvalidGpuId;
                RHI::GpuId distanceAtlas = RHI::kInvalidGpuId;
                RHI::GpuId ubo = RHI::kInvalidGpuId;
                int allocatedAtlasW = 0;
                int allocatedAtlasH = 0;
                bool gpuReady = false;
            };
#pragma warning(pop)

        }
    }
}
