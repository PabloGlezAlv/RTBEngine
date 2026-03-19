#include "Frustum.h"
#include "Frustum.h"
#include <cmath>
#include <algorithm>

namespace RTBEngine {
    namespace Rendering {

        Frustum::Frustum() {}
        Frustum::~Frustum() {}

        void Frustum::ExtractPlanes(const Math::Matrix4& viewProjection) {
            const float* m = viewProjection.GetData();

            // Raw plane coefficients (a, b, c, d) from VP matrix rows
            float rawPlanes[6][4] = {
                { m[3] + m[0], m[7] + m[4], m[11] + m[8],  m[15] + m[12] },  // Left
                { m[3] - m[0], m[7] - m[4], m[11] - m[8],  m[15] - m[12] },  // Right
                { m[3] + m[1], m[7] + m[5], m[11] + m[9],  m[15] + m[13] },  // Bottom
                { m[3] - m[1], m[7] - m[5], m[11] - m[9],  m[15] - m[13] },  // Top
                { m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14] },  // Near
                { m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14] }   // Far
            };

            for (int i = 0; i < 6; i++) {
                float length = std::sqrt(rawPlanes[i][0] * rawPlanes[i][0] +
                    rawPlanes[i][1] * rawPlanes[i][1] +
                    rawPlanes[i][2] * rawPlanes[i][2]);
                if (length > 0.0f) {
                    float invLength = 1.0f / length;
                    planes[i].normal = Math::Vector3(rawPlanes[i][0] * invLength,
                        rawPlanes[i][1] * invLength,
                        rawPlanes[i][2] * invLength);
                    planes[i].distance = rawPlanes[i][3] * invLength;
                }
            }
        }

        bool Frustum::IsAABBVisible(const Math::Vector3& aabbMin, const Math::Vector3& aabbMax) const {
            for (int i = 0; i < 6; i++) {
                Math::Vector3 pVertex(
                    (planes[i].normal.x >= 0.0f) ? aabbMax.x : aabbMin.x,
                    (planes[i].normal.y >= 0.0f) ? aabbMax.y : aabbMin.y,
                    (planes[i].normal.z >= 0.0f) ? aabbMax.z : aabbMin.z
                );

                if (planes[i].normal.Dot(pVertex) + planes[i].distance < 0.0f) {
                    return false;
                }
            }
            return true;
        }

        void Frustum::TransformAABB(const Math::Matrix4& worldMatrix,
            const Math::Vector3& localMin,
            const Math::Vector3& localMax,
            Math::Vector3& outWorldMin,
            Math::Vector3& outWorldMax) {
            const float* m = worldMatrix.GetData();

            // Start with translation (column 3)
            float minVal[3] = { m[12], m[13], m[14] };
            float maxVal[3] = { m[12], m[13], m[14] };

            float localMinArr[3] = { localMin.x, localMin.y, localMin.z };
            float localMaxArr[3] = { localMax.x, localMax.y, localMax.z };

            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    float e = m[i + j * 4] * localMinArr[j];
                    float f = m[i + j * 4] * localMaxArr[j];
                    minVal[i] += std::min(e, f);
                    maxVal[i] += std::max(e, f);
                }
            }

            outWorldMin = Math::Vector3(minVal[0], minVal[1], minVal[2]);
            outWorldMax = Math::Vector3(maxVal[0], maxVal[1], maxVal[2]);
        }

    }
}