#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Rendering/PostProcess/VolumeProfile.h"
#include "../Math/Vectors/Vector3.h"
#include "../Reflection/PropertyMacros.h"
#include <string>

namespace RTBEngine {
    namespace Scene {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API VolumeComponent : public Component {
        public:
            VolumeComponent();
            ~VolumeComponent() override = default;

            bool isGlobal = true;
            Math::Vector3 size = Math::Vector3(20.0f, 10.0f, 20.0f);
            int priority = 0;
            float blendDistance = 2.0f;
            float weight = 1.0f;
            std::string profileAssetPath;

            // Distance Fog effect (A) — one override for the whole effect.
            bool overrideDistanceFog = false;
            bool fogEnabled = true;
            Math::Vector3 fogColor = Math::Vector3(0.55f, 0.62f, 0.72f);
            float fogDensity = 0.018f;
            float fogHeight = 0.0f;
            float fogHeightFalloff = 0.08f;
            float fogStart = 8.0f;
            float fogEnd = 140.0f;

            // Volumetric Fog / god rays (B) — one override for the whole effect.
            bool overrideVolumetricFog = false;
            bool volumetricFogEnabled = true;
            float volumetricIntensity = 0.45f;
            float volumetricAnisotropy = 0.55f;
            int volumetricSamples = 16;
            float volumetricMaxLuminance = 0.85f;

            Rendering::VolumeProfile ToProfile() const;

            RTB_COMPONENT(VolumeComponent)
        };
#pragma warning(pop)

    }
}
