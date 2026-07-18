#pragma once

#include "../RTBEngineAPI.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace RTBEngine {
    namespace ECS {

        class World;

        enum class SystemPhase : std::uint8_t {
            Simulation = 0,
            Fixed,
            Presentation
        };

        using SystemTickFn = std::function<void(World& world, float deltaTime)>;

        class RTB_API SystemScheduler {
        public:
            void Register(SystemPhase phase, SystemTickFn tickFn);
            void Tick(SystemPhase phase, World& world, float deltaTime);
            void Clear();

        private:
            struct ScheduledSystem {
                SystemPhase phase = SystemPhase::Simulation;
                SystemTickFn tick;
            };

            std::vector<ScheduledSystem> systems;
        };

    }
}
