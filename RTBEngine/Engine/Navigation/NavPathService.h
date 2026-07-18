#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "NavGrid.h"
#include "NavPathfinder.h"
#include <cstdint>
#include <deque>
#include <vector>

namespace RTBEngine {
    namespace Scene {
        class NavAgentComponent;
        class Scene;
    }

    namespace Navigation {

        // Central navigation runtime: active grid, registered agents, path request budget.
        class RTB_API NavPathService {
        public:
            static NavPathService& GetInstance();

            void SetActiveGrid(NavGrid* grid);
            NavGrid* GetActiveGrid() const { return activeGrid; }

            void RegisterAgent(Scene::NavAgentComponent* agent);
            void UnregisterAgent(Scene::NavAgentComponent* agent);

            // Throttled async path work (processed in Scene::FixedUpdate).
            void QueuePathRequest(Scene::NavAgentComponent* agent);
            void ProcessFixedUpdate();

            // Immediate path solve — used on spawn / first SetDestination when waypoints are empty.
            void ProcessAgentPathNow(Scene::NavAgentComponent* agent);

            const std::vector<Scene::NavAgentComponent*>& GetRegisteredAgents() const { return agents; }

            void SetDebugEnabled(bool enabled);
            bool IsDebugEnabled() const { return debugEnabled; }

            const Scene::NavAgentComponent* GetDebugAgent() const { return debugAgent; }
            void SetDebugAgent(const Scene::NavAgentComponent* agent) { debugAgent = agent; }

        private:
            NavPathService() = default;

            NavGrid* activeGrid = nullptr;
            NavPathfinder pathfinder;
            std::vector<Scene::NavAgentComponent*> agents;
            std::deque<Scene::NavAgentComponent*> pendingRequests;
            bool debugEnabled = false;
            const Scene::NavAgentComponent* debugAgent = nullptr;
        };

        RTB_API void ProcessSceneNavigationFixedUpdate(Scene::Scene* scene);

        RTB_API void SetNavGridDebugEnabled(bool enabled);
        RTB_API bool IsNavGridDebugEnabled();
        RTB_API const NavGrid* GetActiveNavGridForDebug();

    }
}
