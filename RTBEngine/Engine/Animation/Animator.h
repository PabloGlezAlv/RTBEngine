#pragma once
#include "../ECS/Component.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>
#include <unordered_map>

namespace RTBEngine {
    namespace Rendering {
        class Mesh;
    }
}

namespace RTBEngine {
    namespace Animation {

        class Animator : public ECS::Component {
        public:
            Animator();
            virtual ~Animator();

            // Component interface
            virtual void OnStart() override;
            virtual void OnUpdate(float deltaTime) override;

            // Skeleton
            void SetSkeleton(std::shared_ptr<Skeleton> skel);
            Skeleton* GetSkeleton() const { return skeleton.get(); }

            // Animation clips
            void AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
            AnimationClip* GetClip(const std::string& name) const;

            // Playback control
            void Play(const std::string& clipName, bool loop = true);
            void Stop();
            void Pause();
            void Resume();

            bool IsPlaying() const { return playing; }
            bool IsPaused() const { return paused; }

            void SetSpeed(float spd) { speed = spd; }
            float GetSpeed() const { return speed; }

            float GetCurrentTime() const { return currentTime; }
            const std::string& GetCurrentClipName() const { return currentClipName; }

            // Bone transforms for shader
            const std::vector<Math::Matrix4>& GetBoneTransforms() const { return finalBoneTransforms; }
            bool HasBones() const { return skeleton && skeleton->GetBoneCount() > 0; }

            // Loaded meshes with bone data
            void SetMeshes(const std::vector<Rendering::Mesh*>& loadedMeshes) { meshes = loadedMeshes; }
            const std::vector<Rendering::Mesh*>& GetMeshes() const { return meshes; }
            Rendering::Mesh* GetFirstMesh() const { return meshes.empty() ? nullptr : meshes[0]; }

            // Reflected properties
            std::string currentClipName;
            float speed = 1.0f;
            bool playing = false;
            bool looping = true;

            RTB_COMPONENT(Animator)

        private:
            std::shared_ptr<Skeleton> skeleton;
            std::unordered_map<std::string, std::shared_ptr<AnimationClip>> clips;
            
            AnimationClip* currentClip = nullptr;
            float currentTime = 0.0f;
            bool paused = false;

            std::vector<Math::Matrix4> finalBoneTransforms;
            std::vector<Rendering::Mesh*> meshes;  // Meshes with bone data

            void UpdateBoneTransforms();
        };

    }
}
