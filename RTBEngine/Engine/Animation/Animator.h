#pragma once
#include "../RTBEngineAPI.h"
#include "../ECS/Component.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include "../Reflection/PropertyMacros.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace RTBEngine {
    namespace Rendering {
        class Mesh;
    }
    namespace ECS {
        class Scene;
        class GameObject;
    }
}

namespace RTBEngine {
    namespace Animation {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API Animator : public ECS::Component {
        public:
            Animator();
            virtual ~Animator();

            // Component interface
            virtual void OnAwake() override;
            virtual void OnStart() override;
            virtual void OnUpdate(float deltaTime) override;
            virtual void OnLateUpdate(float deltaTime) override;
            virtual void OnValidate() override;

            // Skeleton
            void SetSkeleton(std::shared_ptr<Skeleton> skel);
            Skeleton* GetSkeleton() const { return skeleton.get(); }

            // Animation clips
            void AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
            bool LoadClipFromFbx(const std::string& alias, const std::string& sourceFbx);
            void ClearClips();
            AnimationClip* GetClip(const std::string& name) const;
            std::vector<std::string> GetClipNames() const;

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
            bool ShouldSkinMesh() const { return playing && HasBones() && !finalBoneTransforms.empty(); }

            // Loaded meshes with bone data
            void SetMeshes(const std::vector<Rendering::Mesh*>& loadedMeshes) { meshes = loadedMeshes; }
            const std::vector<Rendering::Mesh*>& GetMeshes() const { return meshes; }
            Rendering::Mesh* GetFirstMesh() const { return meshes.empty() ? nullptr : meshes[0]; }

            //Bone GameObjects
            void CreateBoneGameObjects(ECS::Scene* scene);
            void SyncBoneGameObjects();
            ECS::GameObject* GetBoneGameObject(const std::string& boneName) const;
            ECS::GameObject* GetBoneGameObject(int boneIndex) const;
            bool AreBoneGOsCreated() const { return boneGOsCreated; }
            bool IsBoneGameObject(const ECS::GameObject* go) const;
            void SelectClip(const std::string& clipName, bool loop = true);

            // Reflected properties (Proxy)
            std::string modelRef;
            std::vector<std::string> additionalModels;
            std::string currentClipName;
            std::string defaultClip;
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
            std::vector<Rendering::Mesh*> meshes;

            //Bone GO sync
            struct LocalTransformSnapshot {
                Math::Vector3 position = Math::Vector3::Zero();
                Math::Quaternion rotation = Math::Quaternion::Identity();
                Math::Vector3 scale = Math::Vector3(1.0f, 1.0f, 1.0f);
            };

            std::vector<Math::Matrix4> currentLocalTransforms;
            std::vector<ECS::GameObject*> boneGameObjects;
            std::unordered_map<ECS::GameObject*, LocalTransformSnapshot> attachmentLocalSnapshots;
            bool boneGOsCreated = false;

            void EnsureModelDataLoaded();
            void UpdateBoneTransforms();
            void ApplyBindPoseTransforms();
            void CaptureBoneAttachmentTransforms();
            void RestoreBoneAttachmentTransforms();
        };
#pragma warning(pop)

    }
}
