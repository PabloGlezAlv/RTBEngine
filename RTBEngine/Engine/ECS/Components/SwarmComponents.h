#pragma once

#include "../../RTBEngineAPI.h"
#include "../../Math/Vectors/Vector3.h"
#include "../../Math/Vectors/Vector4.h"
#include <cstdint>

namespace RTBEngine {
    namespace ECS {

        // Orbiting swarm agent — dense motion only (no hierarchy, no virtuals).
        struct RTB_API SwarmMotion {
            Math::Vector3 center = Math::Vector3::Zero();
            float phase = 0.0f;
            float angularSpeed = 1.2f;
            float orbitRadius = 6.0f;
            float bobAmplitude = 0.75f;
            float bobSpeed = 2.4f;
            float height = 1.5f;
        };

        // Per-agent tint for instanced swarm draws (matches OOP MeshRenderer colorRef).
        struct RTB_API SwarmColor {
            Math::Vector4 rgba = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        struct RTB_API SwarmSimulationStats {
            std::uint32_t activeSwarmCount = 0;
            std::uint64_t simulationTicks = 0;
            double lastSimulationMilliseconds = 0.0;
        };

    }
}
