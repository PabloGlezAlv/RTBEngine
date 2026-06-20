#include "NavPathService.h"

#include "../Scene/NavAgentComponent.h"
#include "../Scene/Scene.h"

#include <algorithm>

namespace RTBEngine {
    namespace Navigation {
        namespace {
            // Cap synchronous A* work spread across agents each physics frame.
            constexpr int kMaxPathRequestsPerFrame = 2;
        }

        NavPathService& NavPathService::GetInstance()
        {
            static NavPathService instance;
            return instance;
        }

        void NavPathService::SetActiveGrid(NavGrid* grid)
        {
            activeGrid = grid;
            // Stale queued requests would reference the previous grid's cell layout.
            pendingRequests.clear();
        }

        void NavPathService::RegisterAgent(ECS::NavAgentComponent* agent)
        {
            if (!agent) {
                return;
            }

            auto it = std::find(agents.begin(), agents.end(), agent);
            if (it == agents.end()) {
                agents.push_back(agent);
            }
        }

        void NavPathService::UnregisterAgent(ECS::NavAgentComponent* agent)
        {
            if (!agent) {
                return;
            }

            agents.erase(std::remove(agents.begin(), agents.end(), agent), agents.end());
            pendingRequests.erase(
                std::remove(pendingRequests.begin(), pendingRequests.end(), agent),
                pendingRequests.end());

            if (debugAgent == agent) {
                debugAgent = nullptr;
            }
        }

        void NavPathService::QueuePathRequest(ECS::NavAgentComponent* agent)
        {
            if (!agent || !activeGrid) {
                return;
            }

            if (std::find(pendingRequests.begin(), pendingRequests.end(), agent) != pendingRequests.end()) {
                return;
            }

            pendingRequests.push_back(agent);
        }

        void NavPathService::ProcessFixedUpdate()
        {
            if (!activeGrid) {
                pendingRequests.clear();
                return;
            }

            int processed = 0;
            while (!pendingRequests.empty() && processed < kMaxPathRequestsPerFrame) {
                ECS::NavAgentComponent* agent = pendingRequests.front();
                pendingRequests.pop_front();

                if (!agent || !agent->IsEnabled() ||
                    !agent->GetOwner() || !agent->GetOwner()->IsActiveInHierarchy()) {
                    continue;
                }

                agent->ProcessPathRequest(*activeGrid, pathfinder);
                ++processed;
            }
        }

        void NavPathService::ProcessAgentPathNow(ECS::NavAgentComponent* agent)
        {
            if (!agent || !activeGrid) {
                return;
            }

            agent->ProcessPathRequest(*activeGrid, pathfinder);
        }

        void ProcessSceneNavigationFixedUpdate(ECS::Scene* /*scene*/)
        {
            NavPathService::GetInstance().ProcessFixedUpdate();
        }

        void NavPathService::SetDebugEnabled(bool enabled)
        {
            debugEnabled = enabled;
        }

        void SetNavGridDebugEnabled(bool enabled)
        {
            NavPathService::GetInstance().SetDebugEnabled(enabled);
        }

        bool IsNavGridDebugEnabled()
        {
            return NavPathService::GetInstance().IsDebugEnabled();
        }

        const NavGrid* GetActiveNavGridForDebug()
        {
            return NavPathService::GetInstance().GetActiveGrid();
        }

    }
}
