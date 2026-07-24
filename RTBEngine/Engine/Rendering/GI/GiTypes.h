#pragma once

#include "../../RTBEngineAPI.h"
#include "../../Math/Vectors/Vector3.h"
#include <cstdint>

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            static constexpr int kDDGIOctahedralSize = 16;
            // +2 for 1-texel border per probe tile (prevents LINEAR bleed across probes).
            static constexpr int kDDGIProbeTexelSize = kDDGIOctahedralSize + 2;
            static constexpr int kDDGIRaysPerProbe = 128;
            static constexpr int kDDGIMaxProbesPerFrame = 256;
            static constexpr int kDDGIMaxGridDim = 32;

            struct DDGISettings {
                Math::Vector3 origin{ -10.0f, 0.0f, -10.0f };
                Math::Vector3 extent{ 20.0f, 8.0f, 20.0f };
                int gridX = 16;
                int gridY = 4;
                int gridZ = 16;
                float hysteresis = 0.55f;
                float normalBias = 0.2f;
                float viewBias = 0.25f;
                float probeRadius = 2.0f;
                bool enabled = true;
            };

            inline int DDGIProbeCount(const DDGISettings& s)
            {
                return s.gridX * s.gridY * s.gridZ;
            }

            inline int DDGIAtlasWidth(const DDGISettings& s)
            {
                return s.gridX * kDDGIProbeTexelSize;
            }

            inline int DDGIAtlasHeight(const DDGISettings& s)
            {
                return s.gridZ * s.gridY * kDDGIProbeTexelSize;
            }

            // std140 layout for DDGI sampling in fragment shader
            struct DDGIUBOData {
                alignas(16) float origin[3];
                float _pad0;
                alignas(16) float spacing[3];
                float _pad1;
                alignas(16) int gridDims[3];
                int enabled;
                float hysteresis;
                float normalBias;
                float viewBias;
                float probeRadius;
                alignas(16) float ambientColor[3];
                float ambientIntensity;
                float ddgiIntensity;
                float _pad2[2];
            };

        }
    }
}
