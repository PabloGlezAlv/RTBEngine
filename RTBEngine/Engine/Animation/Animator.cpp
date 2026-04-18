#include "Animator.h"
#include "../RTBEngine.h"
#include "../Rendering/ModelLoader.h"
#include "../Rendering/FbxBinding.h"
#include "../Core/ResourceManager.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/GameObject.h"
#include "../ECS/Scene.h"
#include "../ECS/SceneManager.h"
#include "../Math/Quaternions/Quaternion.h"
#include <cmath>
#include <algorithm>

namespace RTBEngine {
    namespace Animation {

        using ThisClass = Animator;
        RTB_REGISTER_COMPONENT(Animator)
            RTB_PROPERTY(modelRef)
            RTB_PROPERTY(currentClipName)
            RTB_PROPERTY(defaultClip)
            RTB_PROPERTY(speed)
            RTB_PROPERTY(playing)
            RTB_PROPERTY(looping)
        RTB_END_REGISTER(Animator)

        Animator::Animator()
        {
        }

        Animator::~Animator()
        {
        }

        void Animator::EnsureModelDataLoaded()
        {
            // Lazy-load model data (skeleton, meshes, clips, materials) if we have a modelRef
            // but no runtime data yet (typical for prefab snapshot duplicates).
            if (!modelRef.empty() && !skeleton && clips.empty()) {
                auto& resources = Core::ResourceManager::GetInstance();

                Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelRef);

                if (modelData.skeleton) {
                    SetSkeleton(modelData.skeleton);
                }

                if (!modelData.meshes.empty()) {
                    resources.RegisterMeshes(modelRef, modelData.meshes);
                    SetMeshes(modelData.meshes);
                }

                for (const auto& clip : modelData.animations) {
                    AddClip(clip->GetName(), clip);
                }
            }
        }

        void Animator::OnAwake()
        {
            EnsureModelDataLoaded();
        }

        void Animator::OnValidate()
        {
            EnsureModelDataLoaded();

            // Create bone GOs in Edit mode too so the hierarchy is visible
            if (skeleton && !boneGOsCreated) {
                ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
                if (scene) {
                    CreateBoneGameObjects(scene);
                }
            }
        }

        void Animator::OnStart()
        {
            EnsureModelDataLoaded();

            // Initialize bone transforms array
            if (skeleton) {
                finalBoneTransforms.resize(skeleton->GetBoneCount(), Math::Matrix4());
            }

            // Create bone GameObjects if not already created
            if (skeleton && !boneGOsCreated) {
                ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
                if (scene) {
                    CreateBoneGameObjects(scene);
                }
            }

            if (!defaultClip.empty() && GetClip(defaultClip) != nullptr) {
                Play(defaultClip, true);
            } else if (playing && !clips.empty()) {
                Play(clips.begin()->first, true);
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
            }
        }

        void Animator::AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
        {
            if (clips.find(name) != clips.end()) {
                RTB_WARN("[Animator] Clip name collision: \"" + name + "\" already exists and will be overwritten.");
            }
            clips[name] = clip;
        }

        void Animator::ClearClips()
        {
            clips.clear();
            currentClip = nullptr;
            currentClipName.clear();
            currentTime = 0.0f;
            playing = false;
            paused = false;
        }

        AnimationClip* Animator::GetClip(const std::string& name) const
        {
            auto it = clips.find(name);
            if (it != clips.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        std::vector<std::string> Animator::GetClipNames() const
        {
            std::vector<std::string> names;
            names.reserve(clips.size());
            for (const auto& pair : clips) {
                names.push_back(pair.first);
            }
            std::sort(names.begin(), names.end());
            return names;
        }

        void Animator::Play(const std::string& clipName, bool loop)
        {
            AnimationClip* clip = GetClip(clipName);
            if (!clip) {
                return;
            }

            currentClip = clip;
            currentClipName = clipName;
            currentTime = 0.0f;
            playing = true;
            paused = false;
            looping = loop;

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
            currentLocalTransforms.resize(boneCount);

            // Get interpolated local transform for each bone
            for (size_t i = 0; i < boneCount; i++) {
                const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (bone) {
                    Math::Matrix4 transform;
                    // Pass localBindTransform to use its position when animation has no position data
                    if (currentClip->GetBoneTransform(bone->name, currentTime, transform, &bone->localBindTransform)) {
                        currentLocalTransforms[i] = transform;
                    } else {
                        // Use bind pose local transform for bones without any animation data
                        currentLocalTransforms[i] = bone->localBindTransform;
                    }
                }
            }

            // Calculate final transforms (with hierarchy and offset matrices)
            skeleton->CalculateBoneTransforms(currentLocalTransforms, finalBoneTransforms);

            // Sync bone GameObjects with current local transforms
            SyncBoneGameObjects();
        }

        void Animator::CreateBoneGameObjects(ECS::Scene* scene)
        {
            if (!skeleton || !owner || boneGOsCreated) return;

            size_t boneCount = skeleton->GetBoneCount();
            boneGameObjects.resize(boneCount, nullptr);

            for (size_t i = 0; i < boneCount; i++) {
                const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (!bone) continue;

                auto* boneGO = new ECS::GameObject(bone->name);
                boneGO->SetTransient(true);

                // Parent to owner (root bone) or to parent bone GO
                if (bone->parentIndex < 0) {
                    boneGO->SetParent(owner);
                }
                else if (bone->parentIndex < static_cast<int>(boneGameObjects.size()) && boneGameObjects[bone->parentIndex]) {
                    boneGO->SetParent(boneGameObjects[bone->parentIndex]);
                }
                else {
                    boneGO->SetParent(owner);
                }

                scene->AddGameObject(boneGO);

                // Set initial transform from bind pose
                Math::Vector3 pos;
                Math::Quaternion rot;
                Math::Vector3 scale;
                bone->localBindTransform.Decompose(pos, rot, scale);
                boneGO->GetTransform().SetPosition(pos);
                boneGO->GetTransform().SetRotation(rot);
                boneGO->GetTransform().SetScale(scale);

                boneGameObjects[i] = boneGO;
            }

            boneGOsCreated = true;
        }

        void Animator::SyncBoneGameObjects()
        {
            if (!boneGOsCreated || boneGameObjects.empty()) return;

            size_t count = std::min(currentLocalTransforms.size(), boneGameObjects.size());
            for (size_t i = 0; i < count; i++) {
                ECS::GameObject* boneGO = boneGameObjects[i];
                if (!boneGO) continue;

                Math::Vector3 pos;
                Math::Quaternion rot;
                Math::Vector3 scale;
                currentLocalTransforms[i].Decompose(pos, rot, scale);

                boneGO->GetTransform().SetPosition(pos);
                boneGO->GetTransform().SetRotation(rot);
                boneGO->GetTransform().SetScale(scale);
            }
        }

        ECS::GameObject* Animator::GetBoneGameObject(const std::string& boneName) const
        {
            if (!skeleton) return nullptr;
            int idx = skeleton->GetBoneIndex(boneName);
            if (idx < 0 || idx >= static_cast<int>(boneGameObjects.size())) return nullptr;
            return boneGameObjects[idx];
        }

        ECS::GameObject* Animator::GetBoneGameObject(int boneIndex) const
        {
            if (boneIndex < 0 || boneIndex >= static_cast<int>(boneGameObjects.size())) return nullptr;
            return boneGameObjects[boneIndex];
        }

    }
}
