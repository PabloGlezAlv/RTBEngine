#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Quaternions/Quaternion.h"
#include "Entity.h"
#include <cstdint>

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace Physics {
        class PhysicsWorld;
    }

    namespace ECS {

        struct RTB_API LocalTransform {
            Math::Vector3 position = Math::Vector3::Zero();
            Math::Quaternion rotation = Math::Quaternion::Identity();
            float fixedHeight = 0.0f;
        };

        struct RTB_API ProjectileFlight {
            Math::Vector3 direction = Math::Vector3::Forward();
            float speed = 8.0f;
            float maxDistance = 1.15f;
            float distanceTravelled = 0.0f;
            float radius = 0.55f;
            bool pendingDestroy = false;
            bool shouldStop = false;
        };

        struct RTB_API ProjectilePendingHit {
            bool active = false;
            Scene::GameObject* hitObject = nullptr;
            Math::Vector3 hitPoint = Math::Vector3::Zero();
            float hitFraction = 1.0f;
        };

        struct RTB_API ProjectileVisualLink {
            Scene::GameObject* visual = nullptr;
        };

        struct RTB_API ProjectilePhysicsContext {
            Physics::PhysicsWorld* physicsWorld = nullptr;
            Scene::GameObject* instigator = nullptr;
        };

        struct RTB_API ProjectileSimulationStats {
            std::uint32_t activeProjectileCount = 0;
            std::uint64_t simulationTicks = 0;
            double lastSimulationMilliseconds = 0.0;
        };

    }
}
