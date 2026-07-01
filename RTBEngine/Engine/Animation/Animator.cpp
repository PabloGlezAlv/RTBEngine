#include "Animator.h"
#include "../RTBEngine.h"
#include "../Rendering/ModelLoader.h"
#include "../Rendering/FbxBinding.h"
#include "../Core/ResourceManager.h"
#include "../Scene/MeshRenderer.h"
#include "../Scene/GameObject.h"
#include "../Scene/SceneManager.h"
#include "../Math/Quaternions/Quaternion.h"
#include <cmath>
#include <algorithm>

namespace RTBEngine {
    namespace Animation {

        namespace {
            ECS::GameObject* FindDescendantByName(ECS::GameObject* root, const std::string& name)
            {
                if (!root) {
                    return nullptr;
                }
                if (root->GetName() == name) {
                    return root;
                }
                for (ECS::GameObject* child : root->GetChildren()) {
                    if (ECS::GameObject* found = FindDescendantByName(child, name)) {
                        return found;
                    }
                }
                return nullptr;
            }

            void SetLocalFromWorld(ECS::GameObject* go, const Math::Matrix4& worldMatrix, ECS::GameObject* parent)
            {
                Math::Matrix4 localMatrix = worldMatrix;
                if (parent) {
                    localMatrix = parent->GetWorldMatrix().Inverse() * worldMatrix;
                }

                Math::Vector3 localPos, localScale;
                Math::Quaternion localRot;
                localMatrix.Decompose(localPos, localRot, localScale);
                go->GetTransform().SetPosition(localPos);
                go->GetTransform().SetRotation(localRot);
                go->GetTransform().SetScale(localScale);
            }

            void ReparentPreserveWorld(ECS::GameObject* go, ECS::GameObject* newParent)
            {
                if (!go || go->GetParent() == newParent) {
                    return;
                }

                Math::Matrix4 worldMatrix = go->GetWorldMatrix();
                go->SetParent(newParent);
                SetLocalFromWorld(go, worldMatrix, newParent);
            }

            void ApplyBindPose(ECS::GameObject* boneGO, const Bone* bone)
            {
                if (!boneGO || !bone) {
                    return;
                }

                Math::Vector3 pos;
                Math::Quaternion rot;
                Math::Vector3 scale;
                bone->localBindTransform.Decompose(pos, rot, scale);
                boneGO->GetTransform().SetPosition(pos);
                boneGO->GetTransform().SetRotation(rot);
                boneGO->GetTransform().SetScale(scale);
            }
        }

        using ThisClass = Animator;
        RTB_REGISTER_COMPONENT(Animator)
            RTB_PROPERTY(modelRef)
            RTB_PROPERTY(currentClipName)
            RTB_PROPERTY(defaultClip)
            RTB_PROPERTY(speed)
            RTB_PROPERTY(playing)
            RTB_PROPERTY(looping)
            RTB_PROPERTY_STRING_LIST(additionalModels)
        RTB_END_REGISTER(Animator)

        Animator::Animator()
        {
        }

        Animator::~Animator()
        {
        }

        std::string Animator::NormalizeClipName(const std::string& rawName)
        {
            const size_t pipe = rawName.find('|');
            return pipe != std::string::npos ? rawName.substr(pipe + 1) : rawName;
        }

        bool Animator::AreSourcesCurrent() const
        {
            return loadedPrimaryPath == modelRef && loadedAdditionalPaths == additionalModels;
        }

        void Animator::ReloadClipLibrary()
        {
            const std::string previousDefault = defaultClip;
            const std::string previousCurrent = currentClipName;
            const bool wasPlaying = playing;
            const bool wasLooping = looping;

            clips.clear();
            currentClip = nullptr;
            loadedPrimaryPath.clear();
            loadedAdditionalPaths.clear();

            auto& resources = Core::ResourceManager::GetInstance();

            if (!modelRef.empty()) {
                const Rendering::ModelData& modelData = resources.LoadModelData(modelRef);
                if (modelData.skeleton || !modelData.meshes.empty() || !modelData.animations.empty()) {
                    if (modelData.skeleton) {
                        SetSkeleton(modelData.skeleton);
                    }

                    if (!modelData.meshes.empty()) {
                        SetMeshes(modelData.meshes);
                    }

                    for (const auto& clip : modelData.animations) {
                        AddClip(clip->GetName(), clip);
                    }
                } else {
                    RTB_WARN("[Animator] Primary model not found or empty: " + modelRef);
                }
            } else {
                skeleton.reset();
                meshes.clear();
            }

            for (const std::string& addPath : additionalModels) {
                if (addPath.empty()) {
                    continue;
                }

                const Rendering::ModelData& addData = resources.LoadAnimationClips(addPath);
                if (addData.animations.empty()) {
                    RTB_WARN("[Animator] Additional animation source not found or empty: " + addPath);
                    continue;
                }

                for (const auto& clip : addData.animations) {
                    AddClip(clip->GetName(), clip);
                }
            }

            loadedPrimaryPath = modelRef;
            loadedAdditionalPaths = additionalModels;

            defaultClip = NormalizeClipName(previousDefault);
            if (!defaultClip.empty() && GetClip(defaultClip) == nullptr) {
                defaultClip.clear();
            }

            currentClipName = NormalizeClipName(previousCurrent);
            if (!currentClipName.empty()) {
                currentClip = GetClip(currentClipName);
                if (!currentClip) {
                    currentClipName.clear();
                }
            }

            if (!currentClip && !defaultClip.empty()) {
                currentClip = GetClip(defaultClip);
                if (currentClip) {
                    currentClipName = defaultClip;
                }
            }

            playing = wasPlaying && currentClip != nullptr;
            looping = wasLooping;
        }

        void Animator::EnsureSourcesLoaded()
        {
            if (AreSourcesCurrent()) {
                return;
            }

            ReloadClipLibrary();
        }

        void Animator::OnAwake()
        {
            EnsureSourcesLoaded();
        }

        void Animator::OnValidate()
        {
            EnsureSourcesLoaded();

            // Create bone GOs in Edit mode too so the hierarchy is visible
            if (skeleton && !boneGOsCreated) {
                ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
                if (scene) {
                    CreateBoneGameObjects(scene);
                }
            }
            else if (skeleton && boneGOsCreated && !playing) {
                ApplyBindPoseTransforms();
            }
        }

        void Animator::OnStart()
        {
            EnsureSourcesLoaded();

            // Initialize bone transforms array
            if (skeleton) {
                ApplyBindPoseTransforms();
            }

            // Create bone GameObjects if not already created
            if (skeleton && !boneGOsCreated) {
                ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
                if (scene) {
                    CreateBoneGameObjects(scene);
                }
            }

            if (!currentClipName.empty() && GetClip(currentClipName) != nullptr) {
                if (playing) {
                    Play(currentClipName, looping);
                } else {
                    SelectClip(currentClipName, looping);
                }
            } else if (!defaultClip.empty() && GetClip(defaultClip) != nullptr) {
                if (playing) {
                    Play(defaultClip, looping);
                } else {
                    SelectClip(defaultClip, looping);
                }
            } else if (playing && !clips.empty()) {
                Play(clips.begin()->first, looping);
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
                    holdPose = true;
                }
            }

            UpdateBoneTransforms();
        }

        void Animator::OnLateUpdate(float deltaTime)
        {
            (void)deltaTime;
            if (((playing && !paused) || holdPose) && currentClip && skeleton && boneGOsCreated) {
                SyncBoneGameObjects();
            }
        }

        void Animator::SetSkeleton(std::shared_ptr<Skeleton> skel)
        {
            skeleton = skel;
            if (skeleton) {
                ApplyBindPoseTransforms();
            }
        }

        void Animator::AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
        {
            if (!clip || name.empty()) {
                return;
            }

            const std::string normalizedName = NormalizeClipName(name);
            const auto existing = clips.find(normalizedName);
            if (existing != clips.end() && existing->second == clip) {
                return;
            }

            clips[normalizedName] = clip;

            if (currentClipName == normalizedName) {
                currentClip = clip.get();
                if (!currentClip) {
                    currentClipName.clear();
                    currentTime = 0.0f;
                    playing = false;
                    paused = false;
                    return;
                }

                if (currentTime > currentClip->GetDuration()) {
                    currentTime = currentClip->GetDuration();
                }
            }
        }

        bool Animator::LoadClipFromFbx(const std::string& alias, const std::string& sourceFbx)
        {
            if (alias.empty() || sourceFbx.empty()) {
                return false;
            }

            std::string modelPath = sourceFbx;
            std::string clipName;
            const size_t clipSeparator = sourceFbx.find('|');
            if (clipSeparator != std::string::npos) {
                modelPath = sourceFbx.substr(0, clipSeparator);
                clipName = sourceFbx.substr(clipSeparator + 1);
            }

            auto& resources = Core::ResourceManager::GetInstance();
            const Rendering::ModelData& modelData = resources.LoadModelData(modelPath);
            if (modelData.animations.empty()) {
                return false;
            }

            std::shared_ptr<AnimationClip> selectedClip;
            if (clipName.empty()) {
                selectedClip = modelData.animations.front();
            }
            else {
                for (const auto& clip : modelData.animations) {
                    if (clip && NormalizeClipName(clip->GetName()) == NormalizeClipName(clipName)) {
                        selectedClip = clip;
                        break;
                    }
                }
            }

            if (!selectedClip) {
                return false;
            }

            AddClip(alias, selectedClip);
            return true;
        }

        void Animator::ClearClips()
        {
            clips.clear();
            loadedPrimaryPath.clear();
            loadedAdditionalPaths.clear();
            currentClip = nullptr;
            currentClipName.clear();
            currentTime = 0.0f;
            playing = false;
            paused = false;
            holdPose = false;
        }

        AnimationClip* Animator::GetClip(const std::string& name) const
        {
            const std::string normalizedName = NormalizeClipName(name);
            auto it = clips.find(normalizedName);
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

        void Animator::SelectClip(const std::string& clipName, bool loop)
        {
            const std::string normalizedName = NormalizeClipName(clipName);
            AnimationClip* clip = GetClip(normalizedName);
            if (!clip) {
                return;
            }

            currentClip = clip;
            currentClipName = normalizedName;
            currentTime = 0.0f;
            paused = false;
            holdPose = false;
            looping = loop;
        }

        void Animator::Play(const std::string& clipName, bool loop)
        {
            const std::string normalizedName = NormalizeClipName(clipName);
            AnimationClip* clip = GetClip(normalizedName);
            if (!clip) {
                return;
            }

            currentClip = clip;
            currentClipName = normalizedName;
            currentTime = 0.0f;
            playing = true;
            paused = false;
            holdPose = false;
            looping = loop;

            UpdateBoneTransforms();
        }

        void Animator::Stop()
        {
            EnsureSourcesLoaded();

            playing = false;
            paused = false;
            holdPose = false;
            currentTime = 0.0f;
            currentClip = nullptr;
            currentClipName.clear();

            ApplyBindPoseTransforms();
        }

        void Animator::ApplyBindPoseTransforms()
        {
            if (!skeleton) {
                return;
            }

            const size_t boneCount = skeleton->GetBoneCount();
            currentLocalTransforms.resize(boneCount);

            for (size_t i = 0; i < boneCount; ++i) {
                const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (bone) {
                    currentLocalTransforms[i] = bone->localBindTransform;
                }
            }

            skeleton->CalculateBoneTransforms(currentLocalTransforms, finalBoneTransforms);

            if (boneGOsCreated) {
                SyncBoneGameObjects();
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

        void Animator::HoldCurrentPose()
        {
            if (!skeleton || !currentClip) {
                return;
            }

            const float duration = currentClip->GetDuration();
            if (currentTime > duration) {
                currentTime = duration;
            }

            playing = false;
            paused = false;
            holdPose = true;
            UpdateBoneTransforms();
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

        bool Animator::IsBoneGameObject(const ECS::GameObject* go) const
        {
            if (!go) {
                return false;
            }

            for (ECS::GameObject* boneGO : boneGameObjects) {
                if (boneGO == go) {
                    return true;
                }
            }
            return false;
        }

        void Animator::CreateBoneGameObjects(ECS::Scene* scene)
        {
            EnsureSourcesLoaded();

            if (!skeleton || !owner || boneGOsCreated || !scene) {
                return;
            }

            const size_t boneCount = skeleton->GetBoneCount();
            boneGameObjects.assign(boneCount, nullptr);

            std::vector<bool> assigned(boneCount, false);
            size_t remaining = boneCount;

            while (remaining > 0) {
                size_t progress = 0;

                for (size_t i = 0; i < boneCount; ++i) {
                    if (assigned[i]) {
                        continue;
                    }

                    const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                    if (!bone) {
                        assigned[i] = true;
                        --remaining;
                        ++progress;
                        continue;
                    }

                    if (bone->parentIndex >= 0) {
                        if (bone->parentIndex >= static_cast<int>(boneCount) || !assigned[bone->parentIndex]) {
                            continue;
                        }
                    }

                    ECS::GameObject* parentGO = owner;
                    if (bone->parentIndex >= 0) {
                        parentGO = boneGameObjects[bone->parentIndex];
                    }
                    if (!parentGO) {
                        continue;
                    }

                    ECS::GameObject* boneGO = FindDescendantByName(owner, bone->name);
                    const bool createdNew = boneGO == nullptr;
                    if (createdNew) {
                        boneGO = new ECS::GameObject(bone->name);
                        scene->AddGameObject(boneGO);
                        ApplyBindPose(boneGO, bone);
                    }

                    ReparentPreserveWorld(boneGO, parentGO);
                    ApplyBindPose(boneGO, bone);

                    boneGameObjects[i] = boneGO;
                    boneGO->SetAnimatorBone(true);
                    assigned[i] = true;
                    --remaining;
                    ++progress;
                }

                if (progress == 0) {
                    RTB_WARN("[Animator] Failed to resolve full bone hierarchy for \"" + owner->GetName() + "\".");
                    break;
                }
            }

            boneGOsCreated = true;

            if (!currentClip && !currentClipName.empty()) {
                currentClip = GetClip(currentClipName);
            }
            if (!currentClip && !defaultClip.empty()) {
                currentClip = GetClip(defaultClip);
                if (currentClip) {
                    currentClipName = defaultClip;
                }
            }

            ApplyBindPoseTransforms();
            if (currentClip && playing) {
                UpdateBoneTransforms();
            }
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
