#include "Animator.h"
#include "../RTBEngine.h"
#include "../Rendering/ModelLoader.h"
#include "../Rendering/FbxBinding.h"
#include "../Core/ResourceManager.h"
#include "../ECS/MeshRenderer.h"
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
            // but no runtime data yet (típico en duplicados por prefab snapshot).
            if (!modelRef.empty() && !skeleton && clips.empty()) {
                auto& resources = Core::ResourceManager::GetInstance();

                Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelRef);
                RTB_INFO(std::string("[ENSURE_MODEL] LoadModelWithAnimations returned meshes=") + std::to_string(modelData.meshes.size()) +
                    " anims=" + std::to_string(modelData.animations.size()) +
                    " skeleton=" + (modelData.skeleton ? "valid" : "null"));

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

                // Intentar sincronizar el MeshRenderer del mismo GameObject con los datos del FBX
                ECS::GameObject* owner = GetOwner();
                RTB_INFO(std::string("[ENSURE_MODEL] owner=") + (owner ? owner->GetName() : "null"));
                if (owner) {
                    ECS::MeshRenderer* meshRenderer = owner->GetComponent<ECS::MeshRenderer>();
                    RTB_INFO(std::string("[ENSURE_MODEL] meshRenderer=") + (meshRenderer ? "found" : "null"));
                    if (meshRenderer && !modelData.meshes.empty()) {
                        meshRenderer->SetMeshes(modelData.meshes);
                        RTB_INFO(std::string("[ENSURE_MODEL] SetMeshes called on MeshRenderer with ") + std::to_string(modelData.meshes.size()) + " meshes");

                        if (!modelData.materials.empty()) {
                            // Reutilizar la utilidad compartida de binding
                            Rendering::FbxBindingContext ctx{ resources, modelRef, modelData };
                            Rendering::FbxBindingResult bind = Rendering::BuildMeshesAndMaterials(ctx);

                            // Aplicar shader actual del renderer o básico por defecto
                            Rendering::Shader* shader =
                                meshRenderer->GetMeshMaterial(0)
                                    ? meshRenderer->GetMeshMaterial(0)->GetShader()
                                    : resources.GetShader("basic");

                            for (Rendering::Material* mat : bind.meshMaterials) {
                                if (mat) {
                                    mat->SetShader(shader);
                                }
                            }

                            meshRenderer->SetMeshMaterials(bind.meshMaterials);
                        }
                    }
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
        }

        void Animator::OnStart()
        {
            EnsureModelDataLoaded();

            // Initialize bone transforms array
            if (skeleton) {
                finalBoneTransforms.resize(skeleton->GetBoneCount(), Math::Matrix4());
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
            std::vector<Math::Matrix4> localTransforms(boneCount);

            // Get interpolated local transform for each bone
            for (size_t i = 0; i < boneCount; i++) {
                const Bone* bone = skeleton->GetBone(static_cast<int>(i));
                if (bone) {
                    Math::Matrix4 transform;
                    // Pass localBindTransform to use its position when animation has no position data
                    if (currentClip->GetBoneTransform(bone->name, currentTime, transform, &bone->localBindTransform)) {
                        localTransforms[i] = transform;
                    } else {
                        // Use bind pose local transform for bones without any animation data
                        localTransforms[i] = bone->localBindTransform;
                    }
                }
            }

            // Calculate final transforms (with hierarchy and offset matrices)
            skeleton->CalculateBoneTransforms(localTransforms, finalBoneTransforms);
        }

    }
}
