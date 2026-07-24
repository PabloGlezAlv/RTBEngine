#include "LightingProjectSettings.h"
#include "../../Core/Logger.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace RTBEngine {
    namespace Rendering {

        namespace {

            std::string Trim(const std::string& value)
            {
                const auto first = std::find_if_not(value.begin(), value.end(),
                    [](unsigned char c) { return std::isspace(c) != 0; });
                if (first == value.end()) return {};
                const auto last = std::find_if_not(value.rbegin(), value.rend(),
                    [](unsigned char c) { return std::isspace(c) != 0; }).base();
                return std::string(first, last);
            }

            bool ParseBool(const std::string& value, bool defaultValue)
            {
                const std::string trimmed = Trim(value);
                if (trimmed.empty()) return defaultValue;
                if (trimmed == "1" || trimmed == "true" || trimmed == "True" || trimmed == "TRUE") return true;
                if (trimmed == "0" || trimmed == "false" || trimmed == "False" || trimmed == "FALSE") return false;
                return defaultValue;
            }

            float ParseFloat(const std::string& value, float defaultValue)
            {
                const std::string trimmed = Trim(value);
                if (trimmed.empty()) return defaultValue;
                try {
                    return std::stof(trimmed);
                } catch (...) {
                    return defaultValue;
                }
            }

            int ParseInt(const std::string& value, int defaultValue)
            {
                const std::string trimmed = Trim(value);
                if (trimmed.empty()) return defaultValue;
                try {
                    return std::stoi(trimmed);
                } catch (...) {
                    return defaultValue;
                }
            }

        }

        LightingProjectSettings& LightingProjectSettings::Get()
        {
            static LightingProjectSettings instance;
            return instance;
        }

        LightingProjectSettings::LightingProjectSettings()
        {
            ResetToDefaults();
        }

        std::filesystem::path LightingProjectSettings::GetDefaultSettingsFileName()
        {
            return std::filesystem::path("lighting.ini");
        }

        void LightingProjectSettings::ResetToDefaults()
        {
            ambientColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            ambientIntensity = 0.08f;
            ddgiIntensity = 0.32f;
            shadowsEnabled = true;
            shadowMapResolution = 2048;
            ddgiEnabled = true;
            ddgiOrigin = Math::Vector3(-15.0f, 0.0f, -9.0f);
            ddgiExtent = Math::Vector3(30.0f, 12.0f, 18.0f);
            ddgiGridX = 16;
            ddgiGridY = 4;
            ddgiGridZ = 12;
            ddgiHysteresis = 0.55f;
            ddgiNormalBias = 0.2f;
            ddgiViewBias = 0.25f;
            ddgiProbeRadius = 2.0f;
        }

        int LightingProjectSettings::ClampShadowMapResolution(int resolution)
        {
            static constexpr int kOptions[] = { 512, 1024, 2048, 4096, 8192, 16384 };
            int best = kOptions[0];
            int bestDiff = std::abs(resolution - best);
            for (int option : kOptions) {
                const int diff = std::abs(resolution - option);
                if (diff < bestDiff) {
                    best = option;
                    bestDiff = diff;
                }
            }
            return best;
        }

        int LightingProjectSettings::GetClampedShadowMapResolution() const
        {
            return ClampShadowMapResolution(shadowMapResolution);
        }

        GI::DDGISettings LightingProjectSettings::GetDDGISettings() const
        {
            GI::DDGISettings settings{};
            settings.origin = ddgiOrigin;
            settings.extent = ddgiExtent;
            settings.gridX = ddgiGridX;
            settings.gridY = ddgiGridY;
            settings.gridZ = ddgiGridZ;
            settings.hysteresis = ddgiHysteresis;
            settings.normalBias = ddgiNormalBias;
            settings.viewBias = ddgiViewBias;
            settings.probeRadius = ddgiProbeRadius;
            settings.enabled = ddgiEnabled;
            return settings;
        }

        void LightingProjectSettings::SetDDGISettings(const GI::DDGISettings& settings)
        {
            ddgiOrigin = settings.origin;
            ddgiExtent = settings.extent;
            ddgiGridX = settings.gridX;
            ddgiGridY = settings.gridY;
            ddgiGridZ = settings.gridZ;
            ddgiHysteresis = settings.hysteresis;
            ddgiNormalBias = settings.normalBias;
            ddgiViewBias = settings.viewBias;
            ddgiProbeRadius = settings.probeRadius;
            ddgiEnabled = settings.enabled;
        }

        bool LightingProjectSettings::LoadFromFile(const std::filesystem::path& path)
        {
            std::ifstream file(path);
            if (!file.is_open()) {
                return false;
            }

            std::string line;
            while (std::getline(file, line)) {
                const std::size_t eq = line.find('=');
                if (eq == std::string::npos) continue;

                const std::string key = Trim(line.substr(0, eq));
                const std::string value = line.substr(eq + 1);

                if (key == "AmbientColorR") ambientColor.x = ParseFloat(value, ambientColor.x);
                else if (key == "AmbientColorG") ambientColor.y = ParseFloat(value, ambientColor.y);
                else if (key == "AmbientColorB") ambientColor.z = ParseFloat(value, ambientColor.z);
                else if (key == "AmbientIntensity") ambientIntensity = ParseFloat(value, ambientIntensity);
                else if (key == "DDGIIntensity") ddgiIntensity = ParseFloat(value, ddgiIntensity);
                else if (key == "ShadowsEnabled") shadowsEnabled = ParseBool(value, shadowsEnabled);
                else if (key == "ShadowMapResolution") shadowMapResolution = ClampShadowMapResolution(ParseInt(value, shadowMapResolution));
                else if (key == "DDGIEnabled") ddgiEnabled = ParseBool(value, ddgiEnabled);
                else if (key == "DDGIOriginX") ddgiOrigin.x = ParseFloat(value, ddgiOrigin.x);
                else if (key == "DDGIOriginY") ddgiOrigin.y = ParseFloat(value, ddgiOrigin.y);
                else if (key == "DDGIOriginZ") ddgiOrigin.z = ParseFloat(value, ddgiOrigin.z);
                else if (key == "DDGIExtentX") ddgiExtent.x = ParseFloat(value, ddgiExtent.x);
                else if (key == "DDGIExtentY") ddgiExtent.y = ParseFloat(value, ddgiExtent.y);
                else if (key == "DDGIExtentZ") ddgiExtent.z = ParseFloat(value, ddgiExtent.z);
                else if (key == "DDGIGridX") ddgiGridX = ParseInt(value, ddgiGridX);
                else if (key == "DDGIGridY") ddgiGridY = ParseInt(value, ddgiGridY);
                else if (key == "DDGIGridZ") ddgiGridZ = ParseInt(value, ddgiGridZ);
                else if (key == "DDGIHysteresis") ddgiHysteresis = ParseFloat(value, ddgiHysteresis);
                else if (key == "DDGINormalBias") ddgiNormalBias = ParseFloat(value, ddgiNormalBias);
                else if (key == "DDGIViewBias") ddgiViewBias = ParseFloat(value, ddgiViewBias);
                else if (key == "DDGIProbeRadius") ddgiProbeRadius = ParseFloat(value, ddgiProbeRadius);
            }

            return true;
        }

        bool LightingProjectSettings::SaveToFile(const std::filesystem::path& path) const
        {
            std::ofstream file(path);
            if (!file.is_open()) {
                RTB_ERROR("LightingProjectSettings: could not write " + path.string());
                return false;
            }

            file << "AmbientColorR=" << ambientColor.x << "\n";
            file << "AmbientColorG=" << ambientColor.y << "\n";
            file << "AmbientColorB=" << ambientColor.z << "\n";
            file << "AmbientIntensity=" << ambientIntensity << "\n";
            file << "DDGIIntensity=" << ddgiIntensity << "\n";
            file << "ShadowsEnabled=" << (shadowsEnabled ? "1" : "0") << "\n";
            file << "ShadowMapResolution=" << GetClampedShadowMapResolution() << "\n";
            file << "DDGIEnabled=" << (ddgiEnabled ? "1" : "0") << "\n";
            file << "DDGIOriginX=" << ddgiOrigin.x << "\n";
            file << "DDGIOriginY=" << ddgiOrigin.y << "\n";
            file << "DDGIOriginZ=" << ddgiOrigin.z << "\n";
            file << "DDGIExtentX=" << ddgiExtent.x << "\n";
            file << "DDGIExtentY=" << ddgiExtent.y << "\n";
            file << "DDGIExtentZ=" << ddgiExtent.z << "\n";
            file << "DDGIGridX=" << ddgiGridX << "\n";
            file << "DDGIGridY=" << ddgiGridY << "\n";
            file << "DDGIGridZ=" << ddgiGridZ << "\n";
            file << "DDGIHysteresis=" << ddgiHysteresis << "\n";
            file << "DDGINormalBias=" << ddgiNormalBias << "\n";
            file << "DDGIViewBias=" << ddgiViewBias << "\n";
            file << "DDGIProbeRadius=" << ddgiProbeRadius << "\n";
            return true;
        }

    }
}
