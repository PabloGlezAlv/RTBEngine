#pragma once
#include "../Math/Math.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace RTBEngine {
    namespace Animation {

        struct VectorKey {
            float time;
            Math::Vector3 value;
        };

        struct QuatKey {
            float time;
            Math::Quaternion value;
        };

        struct BoneAnimation {
            std::string boneName;
            std::vector<VectorKey> positionKeys;
            std::vector<QuatKey> rotationKeys;
            std::vector<VectorKey> scaleKeys;
        };

        class AnimationClip {
        public:
            AnimationClip(const std::string& name, float duration, float ticksPerSecond);
            ~AnimationClip() = default;

            void AddBoneAnimation(const BoneAnimation& boneAnim);

            // Get interpolated transform for a bone at given time
            // If localBindPose is provided, uses its position/scale when animation data is static (1 key with zero)
            bool GetBoneTransform(const std::string& boneName, float time, Math::Matrix4& outTransform,
                                  const Math::Matrix4* localBindPose = nullptr) const;

            const std::string& GetName() const { return name; }
            float GetDuration() const { return duration; }
            float GetTicksPerSecond() const { return ticksPerSecond; }
            float GetDurationInSeconds() const { return duration / ticksPerSecond; }

        private:
            std::string name;
            float duration;
            float ticksPerSecond;
            std::vector<BoneAnimation> boneAnimations;
            std::unordered_map<std::string, size_t> boneNameToAnimIndex;

            // Interpolation helpers
            Math::Vector3 InterpolatePosition(const BoneAnimation& anim, float time) const;
            Math::Quaternion InterpolateRotation(const BoneAnimation& anim, float time) const;
            Math::Vector3 InterpolateScale(const BoneAnimation& anim, float time) const;

            template<typename T>
            size_t FindKeyIndex(const std::vector<T>& keys, float time) const;
        };

    }
}
