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

            // Directional shadow maps (OpenGL + Vulkan via ShadowMap / RHI).
            bool shadowsEnabled = true;
            // Preferred size; use GetClampedShadowMapResolution() at runtime (512..16384).
            int shadowMapResolution = 2048;

            int GetClampedShadowMapResolution() const;
            static int ClampShadowMapResolution(int resolution);

            Math::Vector3 ddgiOrigin = Math::Vector3(-15.0f, 0.0f, -9.0f);
            Math::Vector3 ddgiExtent = Math::Vector3(30.0f, 12.0f, 18.0f);
            int ddgiGridX = 16;
            int ddgiGridY = 4;
            int ddgiGridZ = 12;
            float ddgiHysteresis = 0.55f;
            float ddgiNormalBias = 0.2f;
            float ddgiViewBias = 0.25f;
            float ddgiProbeRadius = 2.0f;
            bool ddgiEnabled = false;

            // Distance / height fog (forward shading).
            bool fogEnabled = false;
            Math::Vector3 fogColor = Math::Vector3(0.55f, 0.62f, 0.72f);
            float fogDensity = 0.018f;
            float fogHeight = 0.0f;
            float fogHeightFalloff = 0.08f;
            float fogStart = 8.0f;
            float fogEnd = 140.0f;

            // Fullscreen volumetric in-scattering (god rays) after geometry.
            bool volumetricFogEnabled = false;
            float volumetricIntensity = 0.45f;
            float volumetricAnisotropy = 0.55f;
            int volumetricSamples = 16;
            // Soft luminance ceiling for additive volumetric (prevents outdoor white-out).
            float volumetricMaxLuminance = 0.85f;

        private:
            LightingProjectSettings();
        };

    }
}
