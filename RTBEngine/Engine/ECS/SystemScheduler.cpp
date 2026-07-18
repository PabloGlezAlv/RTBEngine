#include "SystemScheduler.h"
#include "World.h"

namespace RTBEngine {
    namespace ECS {

        void SystemScheduler::Register(SystemPhase phase, SystemTickFn tickFn)
        {
            if (!tickFn) {
                return;
            }

            systems.push_back(ScheduledSystem{ phase, std::move(tickFn) });
        }

        void SystemScheduler::Tick(SystemPhase phase, World& world, float deltaTime)
        {
            for (const ScheduledSystem& system : systems) {
                if (system.phase == phase && system.tick) {
                    system.tick(world, deltaTime);
                }
            }
        }

        void SystemScheduler::Clear()
        {
            systems.clear();
        }

    }
}
