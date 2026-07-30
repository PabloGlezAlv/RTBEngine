#include "VolumeComponent.h"

namespace RTBEngine {
    namespace Scene {

        using ThisClass = VolumeComponent;
        RTB_REGISTER_COMPONENT(VolumeComponent)
            RTB_PROPERTY_HIDDEN(isGlobal)
            RTB_PROPERTY_HIDDEN(size)
            RTB_PROPERTY_HIDDEN(priority)
            RTB_PROPERTY_HIDDEN(blendDistance)
            RTB_PROPERTY_HIDDEN(weight)
            RTB_PROPERTY_HIDDEN(profileAssetPath)
            RTB_PROPERTY_HIDDEN(overrideDistanceFog)
            RTB_PROPERTY_HIDDEN(fogEnabled)
            RTB_PROPERTY_HIDDEN(fogColor)
            RTB_PROPERTY_HIDDEN(fogDensity)
            RTB_PROPERTY_HIDDEN(fogHeight)
            RTB_PROPERTY_HIDDEN(fogHeightFalloff)
            RTB_PROPERTY_HIDDEN(fogStart)
            RTB_PROPERTY_HIDDEN(fogEnd)
            RTB_PROPERTY_HIDDEN(overrideVolumetricFog)
            RTB_PROPERTY_HIDDEN(volumetricFogEnabled)
            RTB_PROPERTY_HIDDEN(volumetricIntensity)
            RTB_PROPERTY_HIDDEN(volumetricAnisotropy)
            RTB_PROPERTY_HIDDEN(volumetricSamples)
            RTB_PROPERTY_HIDDEN(volumetricMaxLuminance)
            RTB_PROPERTY_HIDDEN(overrideBloom)
            RTB_PROPERTY_HIDDEN(bloomEnabled)
            RTB_PROPERTY_HIDDEN(bloomThreshold)
            RTB_PROPERTY_HIDDEN(bloomIntensity)
        RTB_END_REGISTER(VolumeComponent)

        VolumeComponent::VolumeComponent()
            : Component()
        {
        }

        Rendering::VolumeProfile VolumeComponent::ToProfile() const
        {
            Rendering::VolumeProfile profile{};
            profile.overrideDistanceFog = overrideDistanceFog;
            profile.fogEnabled = fogEnabled;
            profile.fogColor = fogColor;
            profile.fogDensity = fogDensity;
            profile.fogHeight = fogHeight;
            profile.fogHeightFalloff = fogHeightFalloff;
            profile.fogStart = fogStart;
            profile.fogEnd = fogEnd;
            profile.overrideVolumetricFog = overrideVolumetricFog;
            profile.volumetricFogEnabled = volumetricFogEnabled;
            profile.volumetricIntensity = volumetricIntensity;
            profile.volumetricAnisotropy = volumetricAnisotropy;
            profile.volumetricSamples = volumetricSamples;
            profile.volumetricMaxLuminance = volumetricMaxLuminance;
            profile.overrideBloom = overrideBloom;
            profile.bloomEnabled = bloomEnabled;
            profile.bloomThreshold = bloomThreshold;
            profile.bloomIntensity = bloomIntensity;
            return profile;
        }

    }
}
