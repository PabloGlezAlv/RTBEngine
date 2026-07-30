#pragma once

#include "../../RTBEngineAPI.h"
#include "../../Math/Vectors/Vector3.h"
#include "FogFrameState.h"

namespace RTBEngine {
    namespace Rendering {

        // Effect-level overrides (Distance Fog / Volumetric Fog / Bloom), not per-parameter.
        struct RTB_API VolumeProfile {
            bool overrideDistanceFog = false;
            bool fogEnabled = true;
            Math::Vector3 fogColor = Math::Vector3(0.55f, 0.62f, 0.72f);
            float fogDensity = 0.018f;
            float fogHeight = 0.0f;
            float fogHeightFalloff = 0.08f;
            float fogStart = 8.0f;
            float fogEnd = 140.0f;

            bool overrideVolumetricFog = false;
            bool volumetricFogEnabled = true;
            float volumetricIntensity = 0.45f;
            float volumetricAnisotropy = 0.55f;
            int volumetricSamples = 16;
            float volumetricMaxLuminance = 0.85f;

            bool overrideBloom = false;
            bool bloomEnabled = true;
            float bloomThreshold = 1.0f;
            float bloomIntensity = 0.8f;

            void BlendInto(FogFrameState& state, float weight) const;
        };

    }
}
