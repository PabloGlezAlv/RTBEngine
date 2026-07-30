#pragma once

#include "../../RTBEngineAPI.h"
#include "../../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Rendering {

        // Per-camera post-process values after project defaults + volume stack blending.
        struct RTB_API FogFrameState {
            bool fogEnabled = false;
            Math::Vector3 fogColor = Math::Vector3(0.55f, 0.62f, 0.72f);
            float fogDensity = 0.018f;
            float fogHeight = 0.0f;
            float fogHeightFalloff = 0.08f;
            float fogStart = 8.0f;
            float fogEnd = 140.0f;

            bool volumetricFogEnabled = false;
            float volumetricIntensity = 0.45f;
            float volumetricAnisotropy = 0.55f;
            int volumetricSamples = 16;
            float volumetricMaxLuminance = 0.85f;

            bool bloomEnabled = true;
            float bloomThreshold = 1.0f;
            float bloomIntensity = 0.8f;

            static FogFrameState FromProjectDefaults();
        };

    }
}
