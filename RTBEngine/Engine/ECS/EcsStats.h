#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace ECS {

        // Generic ECS world metrics (editor overlay, profiling).
        struct RTB_API EcsSimulationStats {
            std::uint32_t aliveEntityCount = 0;
            double lastSimulationMilliseconds = 0.0;
        };

    }
}
