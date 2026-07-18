#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "../Physics/PhysicsWorld.h"
#include "Components/ProjectileComponents.h"
#include "Entity.h"

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace ECS {

        class World;

        RTB_API void RegisterDefaultSystems(World& world);

        RTB_API Entity CreateProjectileEntity(World& world,
                                      Scene::GameObject* visual,
                                      const LocalTransform& transform,
                                      const ProjectileFlight& flight,
                                      const ProjectilePhysicsContext& physicsContext);

        RTB_API void DestroyProjectileEntity(World& world, Entity entity);

    }
}
