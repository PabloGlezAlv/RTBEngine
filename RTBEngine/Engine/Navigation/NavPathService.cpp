#include "NavPathService.h"

#include "../ECS/NavAgentComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/CapsuleColliderComponent.h"
#include "../ECS/GameObject.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/Scene.h"
#include "../ECS/SphereColliderComponent.h"

#include <algorithm>

namespace RTBEngine {
    namespace Navigation {
        namespace {
            constexpr int kMaxPathRequestsPerFrame = 2;

            RTBEngine::Physics::PhysicsWorld* ResolvePhysicsWorldFromGameObject(ECS::GameObject* gameObject)
            {
                if (!gameObject) {
                    return nullptr;
                }

                if (auto* rigidBody = gameObject->GetComponent<ECS::RigidBodyComponent>()) {
                    if (rigidBody->GetRigidBody() && rigidBody->GetRigidBody()->GetPhysicsWorld()) {
                        return rigidBody->GetRigidBody()->GetPhysicsWorld();
                    }
                }

                if (auto* boxCollider = gameObject->GetComponent<ECS::BoxColliderComponent>()) {
                    if (boxCollider->GetPhysicsWorld()) {
                        return boxCollider->GetPhysicsWorld();
                    }
                }

                if (auto* sphereCollider = gameObject->GetComponent<ECS::SphereColliderComponent>()) {
                    if (sphereCollider->GetPhysicsWorld()) {
                        return sphereCollider->GetPhysicsWorld();
                    }
                }

                if (auto* capsuleCollider = gameObject->GetComponent<ECS::CapsuleColliderComponent>()) {
                    if (capsuleCollider->GetPhysicsWorld()) {
                        return capsuleCollider->GetPhysicsWorld();
                    }
                }

                for (ECS::GameObject* child : gameObject->GetChildren()) {
                    if (RTBEngine::Physics::PhysicsWorld* world = ResolvePhysicsWorldFromGameObject(child)) {
                        return world;
                    }
                }

                return nullptr;
            }

            RTBEngine::Physics::PhysicsWorld* ResolvePhysicsWorldFromScene(ECS::Scene* scene)
            {
                if (!scene) {
                    return nullptr;
                }

                for (const auto& gameObject : scene->GetGameObjects()) {
                    if (!gameObject) {
                        continue;
                    }

                    if (RTBEngine::Physics::PhysicsWorld* world = ResolvePhysicsWorldFromGameObject(gameObject.get())) {
                        return world;
                    }
                }

                return nullptr;
            }
        }

        NavPathService& NavPathService::GetInstance()
        {
            static NavPathService instance;
            return instance;
        }

        void NavPathService::SetActiveGrid(NavGrid* grid)
        {
            activeGrid = grid;
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

        void NavPathService::ProcessFixedUpdate(Physics::PhysicsWorld* physicsWorld)
        {
            if (!activeGrid) {
                pendingRequests.clear();
                return;
            }

            Physics::PhysicsWorld* world = physicsWorld;
            if (!world) {
                for (ECS::NavAgentComponent* agent : agents) {
                    if (!agent) {
                        continue;
                    }

                    world = agent->ResolvePhysicsWorld();
                    if (world) {
                        break;
                    }
                }
            }

            int processed = 0;
            while (!pendingRequests.empty() && processed < kMaxPathRequestsPerFrame) {
                ECS::NavAgentComponent* agent = pendingRequests.front();
                pendingRequests.pop_front();

                if (!agent || !agent->IsEnabled() ||
                    !agent->GetOwner() || !agent->GetOwner()->IsActiveInHierarchy()) {
                    continue;
                }

                agent->ProcessPathRequest(*activeGrid, pathfinder, world);
                ++processed;
            }
        }

        void NavPathService::ProcessAgentPathNow(ECS::NavAgentComponent* agent,
                                                 Physics::PhysicsWorld* physicsWorld)
        {
            if (!agent || !activeGrid) {
                return;
            }

            Physics::PhysicsWorld* world = physicsWorld;
            if (!world) {
                world = agent->ResolvePhysicsWorld();
            }

            agent->ProcessPathRequest(*activeGrid, pathfinder, world);
        }

        void ProcessSceneNavigationFixedUpdate(ECS::Scene* scene)
        {
            NavPathService::GetInstance().ProcessFixedUpdate(ResolvePhysicsWorldFromScene(scene));
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
