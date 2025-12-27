#include "Skeleton.h"
#include <cstdio>
#include <algorithm>

namespace RTBEngine {
    namespace Animation {

        void Skeleton::AddBone(const Bone& bone) {
            int index = static_cast<int>(bones.size());
            boneNameToIndex[bone.name] = index;
            bones.push_back(bone);
        }

        int Skeleton::GetBoneIndex(const std::string& name) const {
            auto it = boneNameToIndex.find(name);
            if (it != boneNameToIndex.end()) {
                return it->second;
            }
            return -1;
        }

        Bone* Skeleton::GetBone(int index) {
            if (index >= 0 && index < static_cast<int>(bones.size())) {
                return &bones[index];
            }
            return nullptr;
        }

        const Bone* Skeleton::GetBone(int index) const {
            if (index >= 0 && index < static_cast<int>(bones.size())) {
                return &bones[index];
            }
            return nullptr;
        }

        void Skeleton::SetGlobalInverseTransform(const Math::Matrix4& matrix) {
            globalInverseTransform = matrix;
        }

        void Skeleton::CalculateBoneTransforms(
            const std::vector<Math::Matrix4>& localTransforms,
            std::vector<Math::Matrix4>& outFinalTransforms) const
        {
            size_t boneCount = bones.size();
            outFinalTransforms.resize(boneCount);

            // Temporary storage for global transforms
            std::vector<Math::Matrix4> globalTransforms(boneCount);
            std::vector<bool> processed(boneCount, false);

            // Process bones in dependency order (parents before children)
            size_t processedCount = 0;
            while (processedCount < boneCount) {
                bool madeProgress = false;

                for (size_t i = 0; i < boneCount; i++) {
                    if (processed[i]) continue;

                    const Bone& bone = bones[i];

                    // Can only process if parent is already processed (or no parent)
                    if (bone.parentIndex >= 0 && !processed[bone.parentIndex]) {
                        continue;
                    }

                    // Calculate global transform
                    if (bone.parentIndex >= 0) {
                        globalTransforms[i] = globalTransforms[bone.parentIndex] * localTransforms[i];
                    }
                    else {
                        globalTransforms[i] = localTransforms[i];
                    }

                    // Final transform = GlobalInverse * GlobalTransform * OffsetMatrix
                    outFinalTransforms[i] = globalInverseTransform * globalTransforms[i] * bone.offsetMatrix;

                    processed[i] = true;
                    processedCount++;
                    madeProgress = true;
                }

                // Safety: break if no progress (circular dependency or error)
                if (!madeProgress) {
                    break;
                }
            }
        }

    }
}
