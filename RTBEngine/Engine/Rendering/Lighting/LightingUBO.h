#pragma once

#include "../../RTBEngineAPI.h"
#include "../RHI/RenderTypes.h"
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Rendering {

        class Light;

        static constexpr int kMaxPointLightsUBO = 8;
        static constexpr int kMaxSpotLightsUBO = 8;
        static constexpr unsigned int kLightingUBOBindingPoint = RHI::kLightingUBOBinding;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API LightingUBO {
        public:
            static LightingUBO& GetInstance();

            void Upload(const std::vector<Light*>& lights);
            void Bind() const;

        private:
            LightingUBO();
            ~LightingUBO();

            LightingUBO(const LightingUBO&) = delete;
            LightingUBO& operator=(const LightingUBO&) = delete;

            RHI::GpuId buffer = RHI::kInvalidGpuId;
        };
#pragma warning(pop)

    }
}
