#pragma once

#include "../../RTBEngineAPI.h"
#include "../GI/GiTypes.h"
#include "../../Math/Vectors/Vector3.h"
#include <filesystem>

namespace RTBEngine {
    namespace Rendering {

        class RTB_API LightingProjectSettings {
        public:
            static LightingProjectSettings& Get();

            void ResetToDefaults();
            bool LoadFromFile(const std::filesystem::path& path);
            bool SaveToFile(const std::filesystem::path& path) const;

            static std::filesystem::path GetDefaultSettingsFileName();

            bool IsDDGIEnabled() const { return ddgiEnabled; }
            void SetDDGIEnabled(bool value) { ddgiEnabled = value; }

            GI::DDGISettings GetDDGISettings() const;
            void SetDDGISettings(const GI::DDGISettings& settings);

            // Default ambient (used when DDGI is disabled; also uploaded to DDGI UBO).
            Math::Vector3 ambientColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            float ambientIntensity = 0.08f;
            // Scales SampleDDGI contribution (bounce only).
            float ddgiIntensity = 0.32f;

            Math::Vector3 ddgiOrigin = Math::Vector3(-15.0f, 0.0f, -9.0f);
            Math::Vector3 ddgiExtent = Math::Vector3(30.0f, 12.0f, 18.0f);
            int ddgiGridX = 16;
            int ddgiGridY = 4;
            int ddgiGridZ = 12;
            float ddgiHysteresis = 0.55f;
            float ddgiNormalBias = 0.2f;
            float ddgiViewBias = 0.25f;
            float ddgiProbeRadius = 2.0f;
            bool ddgiEnabled = true;

        private:
            LightingProjectSettings();
        };

    }
}
