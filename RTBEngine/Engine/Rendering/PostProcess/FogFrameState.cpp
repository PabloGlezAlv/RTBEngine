#include "FogFrameState.h"

#include "../Lighting/LightingProjectSettings.h"

namespace RTBEngine {
    namespace Rendering {

        FogFrameState FogFrameState::FromProjectDefaults()
        {
            const auto& s = LightingProjectSettings::Get();
            FogFrameState state{};
            state.fogEnabled = s.fogEnabled;
            state.fogColor = s.fogColor;
            state.fogDensity = s.fogDensity;
            state.fogHeight = s.fogHeight;
            state.fogHeightFalloff = s.fogHeightFalloff;
            state.fogStart = s.fogStart;
            state.fogEnd = s.fogEnd;
            state.volumetricFogEnabled = s.volumetricFogEnabled;
            state.volumetricIntensity = s.volumetricIntensity;
            state.volumetricAnisotropy = s.volumetricAnisotropy;
            state.volumetricSamples = s.volumetricSamples;
            state.volumetricMaxLuminance = s.volumetricMaxLuminance;
            return state;
        }

    }
}
