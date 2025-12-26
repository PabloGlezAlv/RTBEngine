#pragma once
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        // Maximum bones that can influence a single vertex
        constexpr int MAX_BONE_INFLUENCE = 4;

        struct Vertex {
            Math::Vector3 position;     // location 0
            Math::Vector3 normal;       // location 1
            Math::Vector2 texCoords;    // location 2

            // Skeletal animation data
            int boneIndices[MAX_BONE_INFLUENCE] = { 0, 0, 0, 0 };       // location 3
            float boneWeights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };  // location 4

            // Helper to add bone influence to this vertex
            void AddBoneInfluence(int boneIndex, float weight) {
                for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                    if (boneWeights[i] == 0.0f) {
                        boneIndices[i] = boneIndex;
                        boneWeights[i] = weight;
                        return;
                    }
                }
                // If all slots are full, replace the smallest weight if new weight is larger
                int minIndex = 0;
                float minWeight = boneWeights[0];
                for (int i = 1; i < MAX_BONE_INFLUENCE; i++) {
                    if (boneWeights[i] < minWeight) {
                        minWeight = boneWeights[i];
                        minIndex = i;
                    }
                }
                if (weight > minWeight) {
                    boneIndices[minIndex] = boneIndex;
                    boneWeights[minIndex] = weight;
                }
            }

            // Normalize bone weights so they sum to 1.0
            void NormalizeBoneWeights() {
                float totalWeight = 0.0f;
                for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                    totalWeight += boneWeights[i];
                }
                if (totalWeight > 0.0f) {
                    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                        boneWeights[i] /= totalWeight;
                    }
                }
            }
        };

    }
}
