#pragma once
#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include "../Math/Matrix/Matrix4.h"

namespace RTBEngine {
    namespace Rendering {

        class RTB_API Frustum {
        public:
            struct Plane {
                Math::Vector3 normal;
                float distance = 0.0f;
            };

            Frustum();
            ~Frustum();

            Frustum(const Frustum&) = delete;
            Frustum& operator=(const Frustum&) = delete;

            void ExtractPlanes(const Math::Matrix4& viewProjection);
            bool IsAABBVisible(const Math::Vector3& aabbMin, const Math::Vector3& aabbMax) const;

            static void TransformAABB(const Math::Matrix4& worldMatrix,
                const Math::Vector3& localMin,
                const Math::Vector3& localMax,
                Math::Vector3& outWorldMin,
                Math::Vector3& outWorldMax);

        private:
            Plane planes[6];
        };

    }
}
