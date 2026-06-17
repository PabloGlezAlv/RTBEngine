#pragma once

#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace ECS {
        class GameObject;
        class Scene;
    }

    namespace Physics {
        class PhysicsWorld;
    }

    namespace ECS {

        // Finds a physics world from colliders/rigid bodies on a GameObject (optionally its children).
        RTB_API Physics::PhysicsWorld* ResolvePhysicsWorldFromGameObject(
            GameObject* gameObject,
            bool searchChildren = true);

        // Scans scene root GameObjects until a physics world is found.
        RTB_API Physics::PhysicsWorld* ResolvePhysicsWorldFromScene(Scene* scene);

    }
}
