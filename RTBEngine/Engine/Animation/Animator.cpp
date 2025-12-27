#include "Animator.h"
#include <cstdio>
#include <cmath>

namespace RTBEngine {
    namespace Animation {

        Animator::Animator()
        {
        }

        Animator::~Animator()
        {
        }

        void Animator::OnStart()
        {
            // Initialize bone transforms array
            if (skeleton) {
                finalBoneTransforms.resize(skeleton->GetBoneCount(), Math::Matrix4());
            }
        }

        void Animator::OnUpdate(float deltaTime)
        {
            if (!playing || paused || !currentClip || !skeleton) {
                return;
            }

            // Advance time
            currentTime += deltaTime * speed * currentClip->GetTicksPerSecond();

            // Handle looping or stop
            float duration = currentClip->GetDuration();
            if (currentTime >= duration) {
                if (looping) {
                    currentTime = fmod(currentTime, duration);
                }
                else {
                    currentTime = duration;
                    playing = false;
                }
            }

            UpdateBoneTransforms();
        }

        void Animator::SetSkeleton(std::shared_ptr<Skeleton> skel)
        {
            skeleton = skel;
            if (skeleton) {
                finalBoneTransforms.resize(skeleton->GetBoneCount(), Math::Matrix4());
                printf("[Animator] Skeleton set with %zu bones\n", skeleton->GetBoneCount());
            }
        }

        void Animator::AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
        {
            clips[name] = clip;
            printf("[Animator] Added clip '%s'\n", name.c_str());
        }

        AnimationClip* Animator::GetClip(const std::string& name) const
        {
            auto it = clips.find(name);
            if (it != clips.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        void Animator::Play(const std::string& clipName, bool loop)
        {
            AnimationClip* clip = GetClip(clipName);
            if (!clip) {
                printf("[Animator] ERROR: Clip '%s' not found! Available clips: ", clipName.c_str());
                for (const auto& pair : clips) {
                    printf("'%s' ", pair.first.c_str());
                }
                printf("\n");
                return;
            }

            currentClip = clip;
            currentClipName = clipName;
            currentTime = 0.0f;
            playing = true;
            paused = false;
            looping = loop;

            printf("[Animator] Playing '%s' (duration=%.2fs, loop=%s)\n",
                   clipName.c_str(),
                   clip->GetDuration() / clip->GetTicksPerSecond(),
                   loop ? "true" : "false");

            UpdateBoneTransforms();
        }

        void Animator::Stop()
        {
            playing = false;
            paused = false;
            currentTime = 0.0f;
            currentClip = nullptr;
            currentClipName.clear();

            // Reset to identity
            for (auto& transform : finalBoneTransforms) {
                transform = Math::Matrix4();
            }
        }

        void Animator::Pause()
        {
            if (playing) {
                paused = true;
            }
        }

        void Animator::Resume()
        {
            if (playing) {
                paused = false;
            }
        }

        void Animator::UpdateBoneTransforms()
        {
            if (!skeleton || !currentClip) {
                return;
            }

            size_t boneCount = skeleton->GetBoneCount();
            std::vector<Math::Matrix4> localTransforms(boneCount);

            static bool debugPrinted = false;
            int matchedBones = 0;

            // Get interpolated local transform for each bone
            for (size_t i = 0; i < boneCount; i++) {
                const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (bone) {
                    Math::Matrix4 transform;
                    // Pass localBindTransform to use its position when animation has no position data
                    if (currentClip->GetBoneTransform(bone->name, currentTime, transform, &bone->localBindTransform)) {
                        localTransforms[i] = transform;
                        matchedBones++;
                    } else {
                        // Use bind pose local transform for bones without any animation data
                        localTransforms[i] = bone->localBindTransform;
                    }
                }
            }

            if (!debugPrinted) {
                printf("[Animator] Bones matched with animation: %d/%zu\n", matchedBones, boneCount);
                debugPrinted = true;
            }

            // Calculate final transforms (with hierarchy and offset matrices)
            skeleton->CalculateBoneTransforms(localTransforms, finalBoneTransforms);
        }

    }
}
