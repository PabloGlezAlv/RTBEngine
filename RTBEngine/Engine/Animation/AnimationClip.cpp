#include "AnimationClip.h"

namespace RTBEngine {
    namespace Animation {

        AnimationClip::AnimationClip(const std::string& name, float duration, float ticksPerSecond)
            : name(name)
            , duration(duration)
            , ticksPerSecond(ticksPerSecond > 0.0f ? ticksPerSecond : 25.0f)
        {
        }

        void AnimationClip::AddBoneAnimation(const BoneAnimation& boneAnim) {

            boneNameToAnimIndex[boneAnim.boneName] = boneAnimations.size();
            boneAnimations.push_back(boneAnim);
        }

        bool AnimationClip::GetBoneTransform(const std::string& boneName, float time, Math::Matrix4& outTransform,
                                              const Math::Matrix4* localBindPose) const {
            auto it = boneNameToAnimIndex.find(boneName);
            if (it == boneNameToAnimIndex.end()) {
                return false;
            }

            const BoneAnimation& anim = boneAnimations[it->second];

            // Get animated values
            Math::Vector3 position = InterpolatePosition(anim, time);
            Math::Quaternion rotation = InterpolateRotation(anim, time);
            Math::Vector3 scale = InterpolateScale(anim, time);

            // If position is static (1 key at zero) and we have bind pose, use bind pose position
            // This handles Mixamo FBX where only root has actual position animation
            if (localBindPose && anim.positionKeys.size() <= 1) {
                // Check if position is essentially zero (static)
                if (std::abs(position.x) < 0.001f && std::abs(position.y) < 0.001f && std::abs(position.z) < 0.001f) {
                    // Use position from bind pose
                    position.x = localBindPose->m[12];
                    position.y = localBindPose->m[13];
                    position.z = localBindPose->m[14];
                }
            }

            // Build transform matrix: T * R * S
            // The rotation from animation is ABSOLUTE in local bone space (not relative to bind pose)
            Math::Matrix4 translationMat = Math::Matrix4::Translate(position);
            Math::Matrix4 rotationMat = rotation.ToMatrix();
            Math::Matrix4 scaleMat = Math::Matrix4::Scale(scale);

            outTransform = translationMat * rotationMat * scaleMat;
            return true;
        }

        template<typename T>
        size_t AnimationClip::FindKeyIndex(const std::vector<T>& keys, float time) const {
            for (size_t i = 0; i < keys.size() - 1; i++) {
                if (time < keys[i + 1].time) {
                    return i;
                }
            }
            return keys.size() - 2;
        }

        Math::Vector3 AnimationClip::InterpolatePosition(const BoneAnimation& anim, float time) const {
            if (anim.positionKeys.empty()) {
                return Math::Vector3(0.0f, 0.0f, 0.0f);
            }
            if (anim.positionKeys.size() == 1) {
                return anim.positionKeys[0].value;
            }

            size_t index = FindKeyIndex(anim.positionKeys, time);
            size_t nextIndex = index + 1;

            float deltaTime = anim.positionKeys[nextIndex].time - anim.positionKeys[index].time;
            float factor = (deltaTime > 0.0f) ? (time - anim.positionKeys[index].time) / deltaTime : 0.0f;
            factor = std::max(0.0f, std::min(1.0f, factor));

            const Math::Vector3& start = anim.positionKeys[index].value;
            const Math::Vector3& end = anim.positionKeys[nextIndex].value;

            return start + (end - start) * factor;
        }

        Math::Quaternion AnimationClip::InterpolateRotation(const BoneAnimation& anim, float time) const {
            if (anim.rotationKeys.empty()) {
                return Math::Quaternion();
            }
            if (anim.rotationKeys.size() == 1) {
                return anim.rotationKeys[0].value;
            }

            size_t index = FindKeyIndex(anim.rotationKeys, time);
            size_t nextIndex = index + 1;

            float deltaTime = anim.rotationKeys[nextIndex].time - anim.rotationKeys[index].time;
            float factor = (deltaTime > 0.0f) ? (time - anim.rotationKeys[index].time) / deltaTime : 0.0f;
            factor = std::max(0.0f, std::min(1.0f, factor));

            const Math::Quaternion& start = anim.rotationKeys[index].value;
            const Math::Quaternion& end = anim.rotationKeys[nextIndex].value;

            return Math::Quaternion::Slerp(start, end, factor);
        }

        Math::Vector3 AnimationClip::InterpolateScale(const BoneAnimation& anim, float time) const {
            if (anim.scaleKeys.empty()) {
                return Math::Vector3(1.0f, 1.0f, 1.0f);
            }
            if (anim.scaleKeys.size() == 1) {
                return anim.scaleKeys[0].value;
            }

            size_t index = FindKeyIndex(anim.scaleKeys, time);
            size_t nextIndex = index + 1;

            float deltaTime = anim.scaleKeys[nextIndex].time - anim.scaleKeys[index].time;
            float factor = (deltaTime > 0.0f) ? (time - anim.scaleKeys[index].time) / deltaTime : 0.0f;
            factor = std::max(0.0f, std::min(1.0f, factor));

            const Math::Vector3& start = anim.scaleKeys[index].value;
            const Math::Vector3& end = anim.scaleKeys[nextIndex].value;

            return start + (end - start) * factor;
        }

    }
}
