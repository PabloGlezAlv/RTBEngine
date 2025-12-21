#pragma once
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace ECS {
        class GameObject;
    }

    namespace Physics {

        struct CollisionInfo {
            ECS::GameObject* otherObject = nullptr;
            Math::Vector3 contactPoint;
            Math::Vector3 contactNormal;
            float penetrationDepth = 0.0f;
        };

    }
}