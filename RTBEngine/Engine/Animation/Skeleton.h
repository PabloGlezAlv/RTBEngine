#pragma once
#include "Bone.h"
#include <vector>
#include <unordered_map>

namespace RTBEngine {
    namespace Animation {

        class Skeleton {
        public:
            Skeleton() = default;
            ~Skeleton() = default;

            void AddBone(const Bone& bone);
            int GetBoneIndex(const std::string& name) const;
            Bone* GetBone(int index);
            const Bone* GetBone(int index) const;
            size_t GetBoneCount() const { return bones.size(); }

            void SetGlobalInverseTransform(const Math::Matrix4& matrix);
            const Math::Matrix4& GetGlobalInverseTransform() const { return globalInverseTransform; }

            // Calcular final matirx
            void CalculateBoneTransforms(
                const std::vector<Math::Matrix4>& localTransforms,
                std::vector<Math::Matrix4>& outFinalTransforms) const;

        private:
            std::vector<Bone> bones;
            std::unordered_map<std::string, int> boneNameToIndex;
            Math::Matrix4 globalInverseTransform;
        };

    }
}
