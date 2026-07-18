#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"

namespace RTBEngine {
    namespace Scene {
        class GameObject;
    }

    namespace Physics {

        struct RTB_API CollisionInfo {
            Scene::GameObject* otherObject = nullptr;
            Math::Vector3 contactPoint;
            Math::Vector3 contactNormal;
            float penetrationDepth = 0.0f;
        };

    }
}