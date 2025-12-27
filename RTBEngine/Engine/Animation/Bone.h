#pragma once
#include "../Math/Math.h"
#include <string>

namespace RTBEngine {
    namespace Animation {

        struct Bone {
            std::string name;
            int parentIndex = -1;           // -1 = root bone
            Math::Matrix4 offsetMatrix;     // Inverse bind pose matrix (mesh space to bone space)
            Math::Matrix4 localBindTransform;  // Local transform in bind pose (T-pose)
            Math::Matrix4 localTransform;   // Current local transform (animated)
        };

    }
}
