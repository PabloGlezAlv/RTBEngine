#pragma once

#include "../RTBEngineAPI.h"
#include "Components/SwarmComponents.h"
#include "Components/ProjectileComponents.h"
#include "Entity.h"

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace ECS {

        class World;

        RTB_API void RegisterSwarmSystems(World& world);

        RTB_API Entity CreateSwarmEntity(World& world,
                                         Scene::GameObject* visual,
                                         const LocalTransform& transform,
                                         const SwarmMotion& motion,
                                         const SwarmColor& color = SwarmColor{});

        RTB_API void DestroySwarmEntity(World& world, Entity entity);

    }
}
