#include "VolumeProfile.h"

#include <algorithm>

namespace RTBEngine {
    namespace Rendering {

        namespace {
            float LerpFloat(float from, float to, float t)
            {
                return from + (to - from) * t;
            }

            Math::Vector3 LerpVec3(const Math::Vector3& from, const Math::Vector3& to, float t)
            {
                return Math::Vector3(
                    LerpFloat(from.x, to.x, t),
                    LerpFloat(from.y, to.y, t),
                    LerpFloat(from.z, to.z, t));
            }

            bool LerpBool(bool from, bool to, float t)
            {
                return t >= 0.5f ? to : from;
            }

            int LerpInt(int from, int to, float t)
            {
                return static_cast<int>(LerpFloat(static_cast<float>(from), static_cast<float>(to), t) + 0.5f);
            }
        }

        void VolumeProfile::BlendInto(FogFrameState& state, float weight) const
        {
            const float t = std::clamp(weight, 0.0f, 1.0f);
            if (t <= 1e-5f) {
                return;
            }

            if (overrideDistanceFog) {
                state.fogEnabled = LerpBool(state.fogEnabled, true, t);
                state.fogColor = LerpVec3(state.fogColor, fogColor, t);
                state.fogDensity = LerpFloat(state.fogDensity, fogDensity, t);
                state.fogHeight = LerpFloat(state.fogHeight, fogHeight, t);
                state.fogHeightFalloff = LerpFloat(state.fogHeightFalloff, fogHeightFalloff, t);
                state.fogStart = LerpFloat(state.fogStart, fogStart, t);
                state.fogEnd = LerpFloat(state.fogEnd, fogEnd, t);
            }

            if (overrideVolumetricFog) {
                state.volumetricFogEnabled = LerpBool(state.volumetricFogEnabled, true, t);
                state.volumetricIntensity = LerpFloat(state.volumetricIntensity, volumetricIntensity, t);
                state.volumetricAnisotropy = LerpFloat(state.volumetricAnisotropy, volumetricAnisotropy, t);
                state.volumetricSamples = LerpInt(state.volumetricSamples, volumetricSamples, t);
                state.volumetricMaxLuminance = LerpFloat(state.volumetricMaxLuminance, volumetricMaxLuminance, t);
            }
        }

    }
}
