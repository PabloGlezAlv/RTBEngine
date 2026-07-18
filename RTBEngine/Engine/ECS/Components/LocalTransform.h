#pragma once

#include "../../RTBEngineAPI.h"
#include "../../Math/Vectors/Vector3.h"
#include "../../Math/Quaternions/Quaternion.h"

namespace RTBEngine {
    namespace ECS {

        // Dense world-space transform for ECS entities (no parent/child hierarchy).
        struct RTB_API LocalTransform {
            Math::Vector3 position = Math::Vector3::Zero();
            Math::Quaternion rotation = Math::Quaternion::Identity();
            float fixedHeight = 0.0f;
        };

    }
}
