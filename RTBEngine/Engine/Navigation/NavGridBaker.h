#pragma once

#include "../RTBEngineAPI.h"
#include "NavGrid.h"

namespace RTBEngine {
    namespace Physics {
        class PhysicsWorld;
    }

    namespace Navigation {

        struct RTB_API NavGridBakeSettings {
            float agentRadius = 0.4f;
            float groundProbeHeight = 4.0f;
            float groundProbeDepth = 6.0f;
            float clearanceHeight = 1.8f;
            float maxStepHeight = 0.45f;
            float minGroundNormalY = 0.65f;
        };

        class RTB_API NavGridBaker {
        public:
            bool Bake(NavGrid& grid,
                      Physics::PhysicsWorld& physicsWorld,
                      const NavGridBakeSettings& settings,
                      int* outWalkableCellCount = nullptr) const;
        };

    }
}
