#pragma once
#include "../RTBEngineAPI.h"
#include "../Scene/Component.h"
#include "../Core/Event.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include "../Reflection/PropertyMacros.h"
#include <GL/glew.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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
        struct RTB_API AnimationKeyClip {
            std::string key;
            std::string clipFbxRef;
            bool loop = false;
        };

        struct RTB_API AnimationKeyFinishedEvent {
            std::string key;
        };

        class RTB_API Animator : public ECS::Component {
        public:
            using KeyFinishedCallback = Core::Event<AnimationKeyFinishedEvent>::Callback;

            Animator();
            virtual ~Animator();

            // Component interface
            virtual void OnAwake() override;
            virtual void OnStart() override;
            virtual void OnUpdate(float deltaTime) override;
            virtual void OnLateUpdate(float deltaTime) override;
            virtual void OnValidate() override;
            virtual void OnDestroy() override;

            // --- Clip library (single loading pipeline) ---
            // Strips legacy vendor prefixes (e.g. "mixamo.com|Walk" -> "Walk").
            static std::string NormalizeClipName(const std::string& rawName);
            // Reloads skeleton, meshes, and all clips from modelRef + additionalModels.
            void ReloadClipLibrary();

            // Skeleton
            void SetSkeleton(std::shared_ptr<Skeleton> skel);
            Skeleton* GetSkeleton() const { return skeleton.get(); }

            // Animation clips
            void AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
            bool LoadClipFromFbx(const std::string& alias, const std::string& sourceFbx);
            void ReloadKeyClips();
            bool SetKeyClip(const std::string& key, const std::string& clipFbxRef, bool loop);
            bool HasKey(const std::string& key) const;
            bool PlayKey(const std::string& key);
            bool PlayKey(const std::string& key, bool loop);
            bool IsPlayingKey(const std::string& key) const;
            Core::EventSubscription SubscribeKeyFinished(KeyFinishedCallback callback);
            Core::EventSubscription SubscribeKeyFinished(const std::string& key, KeyFinishedCallback callback);
            void ClearClips();
            AnimationClip* GetClip(const std::string& name) const;
            std::vector<std::string> GetClipNames() const;

            // Playback control
            void Play(const std::string& clipName, bool loop = true);
            void Stop();
            void Pause();
            void Resume();
            void HoldCurrentPose();

            bool IsPlaying() const { return playing; }
            bool IsPaused() const { return paused; }
            bool IsHoldingPose() const { return holdPose; }

            void SetSpeed(float spd) { speed = spd; }
            float GetSpeed() const { return speed; }

            float GetCurrentTime() const { return currentTime; }
            const std::string& GetCurrentClipName() const { return currentClipName; }

            // Bone transforms for shader
            const std::vector<Math::Matrix4>& GetBoneTransforms() const { return finalBoneTransforms; }
            bool HasBones() const { return skeleton && skeleton->GetBoneCount() > 0; }
            bool ShouldSkinMesh() const { return HasBones() && !finalBoneTransforms.empty() && (playing || holdPose); }

            // Uploads the current pose to this animator's bone UBO (only when the pose changed)
            // and binds it to kBoneUBOBindingPoint. Called from the render/shadow passes; every
            // mesh skinned by this animator reuses the same GPU upload.
            void BindBoneMatrices();

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
            std::vector<AnimationKeyClip> keyClips;
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
            bool holdPose = false;

            std::vector<Math::Matrix4> finalBoneTransforms;
            std::vector<Rendering::Mesh*> meshes;

            // Per-animator GPU buffer holding finalBoneTransforms (std140 BoneData block).
            GLuint boneMatricesUBO = 0;
            bool boneMatricesDirty = true;

            std::vector<Math::Matrix4> currentLocalTransforms;
            std::vector<ECS::GameObject*> boneGameObjects;
            bool boneGOsCreated = false;

            std::string loadedPrimaryPath;
            std::vector<std::string> loadedAdditionalPaths;
            std::unordered_set<std::string> sourceDerivedClipNames;
            std::unordered_set<std::string> loadedKeyClipAliases;

            Core::Event<AnimationKeyFinishedEvent> keyFinishedEvent;

            bool AreSourcesCurrent() const;
            const AnimationKeyClip* FindKeyClip(const std::string& key) const;
            void NotifyKeyFinished(const std::string& key);
            void EnsureSourcesLoaded();
            void AddClipFromSource(const std::string& name, std::shared_ptr<AnimationClip> clip);
            void UpdateBoneTransforms();
            void ApplyBindPoseTransforms();
            void ReleaseBoneMatricesUBO();
        };
#pragma warning(pop)

    }
}

