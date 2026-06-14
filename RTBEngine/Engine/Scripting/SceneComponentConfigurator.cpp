#include "SceneComponentConfigurator.h"

#include "SceneParsingUtils.h"
#include "../RTBEngine.h"
#include <lua.hpp>

#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/SphereColliderComponent.h"
#include "../ECS/CapsuleColliderComponent.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"

#include "../Animation/Animator.h"

#include "../Core/ResourceManager.h"

#include "../Rendering/ModelLoader.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include "../Rendering/Material.h"
#include "../Rendering/FbxBinding.h"

#include "../Physics/RigidBody.h"
#include "../Physics/BoxCollider.h"
#include "../Physics/SphereCollider.h"

#include "../UI/Canvas.h"
#include "../UI/UIElement.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UILayoutGroup.h"

#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace Scripting {
        namespace SceneComponentConfigurator {
            using namespace SceneParsingUtils;

            void ConfigureCanvas(lua_State* L, int tableIndex, UI::Canvas* comp) {
                const std::string renderMode = ReadOptionalString(L, tableIndex, "renderMode", "ScreenSpaceOverlay");
                if (renderMode == "WorldSpace") {
                    comp->SetRenderMode(UI::Canvas::RenderMode::WorldSpace);
                } else if (renderMode == "ScreenSpaceCamera") {
                    comp->SetRenderMode(UI::Canvas::RenderMode::ScreenSpaceCamera);
                } else {
                    comp->SetRenderMode(UI::Canvas::RenderMode::ScreenSpaceOverlay);
                }

                comp->SetCanvasSize(ReadOptionalVector2(L, tableIndex, "canvasSize", comp->GetCanvasSize()));
                comp->SetPixelsPerUnit(ReadOptionalFloat(L, tableIndex, "pixelsPerUnit", comp->GetPixelsPerUnit()));
                comp->SetSortOrder(static_cast<int>(ReadOptionalFloat(L, tableIndex, "sortOrder", comp->GetSortOrder())));
                comp->SetFaceCamera(ReadOptionalBool(L, tableIndex, "faceCamera", comp->GetFaceCamera()));
            }

            void SyncUIElementProxies(lua_State* L, int tableIndex, UI::UIElement* comp) {
                comp->SetVisible(ReadOptionalBool(L, tableIndex, "isVisible", comp->IsVisible()));
                comp->SetAnchorMin(ReadOptionalVector2(L, tableIndex, "anchorMin", comp->GetAnchorMin()));
                comp->SetAnchorMax(ReadOptionalVector2(L, tableIndex, "anchorMax", comp->GetAnchorMax()));
                comp->SetPivot(ReadOptionalVector2(L, tableIndex, "pivot", comp->GetPivot()));
                comp->SetAnchoredPosition(ReadOptionalVector2(L, tableIndex, "anchoredPosition", comp->GetAnchoredPosition()));
                comp->SetSizeDelta(ReadOptionalVector2(L, tableIndex, "sizeDelta", comp->GetSizeDelta()));
                comp->SetRotation(ReadOptionalFloat(L, tableIndex, "rotation", comp->GetRotation()));
                comp->SetScale(ReadOptionalVector2(L, tableIndex, "scale", comp->GetScale()));
            }

            void ConfigureUILayout(lua_State* L, int tableIndex, UI::UILayoutGroup* comp) {
                comp->padding = ReadOptionalVector2(L, tableIndex, "padding", comp->padding);
                comp->spacing = ReadOptionalFloat(L, tableIndex, "spacing", comp->spacing);
            }

            void ConfigureUIText(lua_State* L, int tableIndex, UI::UIText* comp) {
                comp->SetText(ReadOptionalString(L, tableIndex, "text", comp->GetText()));
                comp->SetColor(ReadOptionalVector4(L, tableIndex, "color", comp->GetColor()));
                comp->SetFontSize(ReadOptionalFloat(L, tableIndex, "fontSize", comp->GetFontSize()));

                const char* currentAlignment = "Left";
                switch (comp->GetAlignment()) {
                case UI::TextAlignment::Center:
                    currentAlignment = "Center";
                    break;
                case UI::TextAlignment::Right:
                    currentAlignment = "Right";
                    break;
                case UI::TextAlignment::Left:
                default:
                    currentAlignment = "Left";
                    break;
                }

                const std::string alignStr = ReadOptionalString(L, tableIndex, "alignment", currentAlignment);
                if (alignStr == "Center")
                    comp->SetAlignment(UI::TextAlignment::Center);
                else if (alignStr == "Right")
                    comp->SetAlignment(UI::TextAlignment::Right);
                else
                    comp->SetAlignment(UI::TextAlignment::Left);

                SyncUIElementProxies(L, tableIndex, comp);
            }

            void ConfigureUIImage(lua_State* L, int tableIndex, UI::UIImage* comp) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
                const std::string texturePath = ReadOptionalString(L, tableIndex, "texture", "");
                if (!texturePath.empty()) {
                    // .texture assets carry flip metadata; raw images use default flip
                    Rendering::Texture* tex = (texturePath.size() > 8 && texturePath.substr(texturePath.size() - 8) == ".texture")
                        ? resources.LoadTextureAsset(texturePath)
                        : resources.LoadTexture(texturePath);
                    if (tex) comp->SetTexture(tex);
                }

                comp->SetTint(ReadOptionalVector4(L, tableIndex, "tintColor", comp->GetTint()));
                comp->SetPreserveAspect(ReadOptionalBool(L, tableIndex, "preserveAspect", comp->GetPreserveAspect()));

                SyncUIElementProxies(L, tableIndex, comp);
            }

            void ConfigureUIPanel(lua_State* L, int tableIndex, UI::UIPanel* comp) {
                comp->SetBackgroundColor(ReadOptionalVector4(L, tableIndex, "backgroundColor", comp->GetBackgroundColor()));
                comp->SetBorderColor(ReadOptionalVector4(L, tableIndex, "borderColor", comp->borderColor));
                comp->SetBorderThickness(ReadOptionalFloat(L, tableIndex, "borderThickness", comp->borderThickness));
                comp->SetHasBorder(ReadOptionalBool(L, tableIndex, "hasBorder", comp->hasBorder));

                SyncUIElementProxies(L, tableIndex, comp);
            }

            void ConfigureUIButton(lua_State* L, int tableIndex, UI::UIButton* comp) {
                comp->SetNormalColor(ReadOptionalVector4(L, tableIndex, "normalColor", comp->normalColor));
                comp->SetHoveredColor(ReadOptionalVector4(L, tableIndex, "hoveredColor", comp->hoveredColor));
                comp->SetPressedColor(ReadOptionalVector4(L, tableIndex, "pressedColor", comp->pressedColor));
                comp->SetDisabledColor(ReadOptionalVector4(L, tableIndex, "disabledColor", comp->disabledColor));
                // Assign directly to avoid UpdateVisuals() side-effect during scene load
                comp->interactable = ReadOptionalBool(L, tableIndex, "interactable", comp->interactable);
                comp->enableDefaultHoverVisuals = ReadOptionalBool(L, tableIndex, "enableDefaultHoverVisuals", comp->enableDefaultHoverVisuals);
            }

            void ConfigureMeshRenderer(lua_State* L, int tableIndex, ECS::MeshRenderer* comp) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

                const std::string shaderName = ReadOptionalString(L, tableIndex, "shader", "basic");
                Rendering::Shader* shader = resources.GetShader(shaderName);
                if (shader) {
                    comp->SetShader(shader);
                }

                // "model" is the explicit field; when absent, derive from meshRef path
                std::string modelPath = ReadOptionalString(L, tableIndex, "model", "");
                if (modelPath.empty() && comp->meshRef) {
                    modelPath = resources.GetMeshPath(comp->meshRef);
                }
                if (!modelPath.empty()) {
                    Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelPath);

                    if (!modelData.meshes.empty()) {
                        resources.RegisterMeshes(modelPath, modelData.meshes);

                        Rendering::FbxBindingContext ctx{ resources, modelPath, modelData };
                        Rendering::FbxBindingResult bind = Rendering::BuildMeshesAndMaterials(ctx);

                        // Pick mesh by index
                        const int idx = comp->meshIndex;
                        const int clampedIdx = (idx >= 0 && idx < static_cast<int>(modelData.meshes.size())) ? idx : 0;
                        comp->SetMesh(modelData.meshes[clampedIdx]);

                        Rendering::Material* mat = nullptr;
                        if (clampedIdx < static_cast<int>(bind.meshMaterials.size()))
                            mat = bind.meshMaterials[clampedIdx];

                        if (mat) {
                            if (shader) mat->SetShader(shader);
                            comp->SetMaterial(mat);
                        }
                    }
                }

                // User-assigned texture overrides whatever the FBX material set.
                // ApplyLuaTableToComponent already wrote textureRef from Lua but
                // SetMaterial() above may have clobbered it — re-apply it now.
                const std::string texturePath = ReadOptionalString(L, tableIndex, "textureRef", "");
                if (!texturePath.empty()) {
                    Rendering::Texture* tex = (texturePath.size() > 8 && texturePath.substr(texturePath.size() - 8) == ".texture")
                        ? resources.LoadTextureAsset(texturePath)
                        : resources.LoadModelTexture(texturePath);
                    if (tex) comp->SetTexture(tex);
                }
            }

            void ConfigureLightComponent(lua_State* L, int tableIndex, ECS::LightComponent* comp) {
                const int lightTypeInt = ReadOptionalInt(L, tableIndex, "lightType", 0);
                comp->lightType = static_cast<Rendering::LightType>(lightTypeInt);

                const Math::Vector4 colorV4 = ReadOptionalVector4(L, tableIndex, "color", Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                comp->color = Math::Color(colorV4.x, colorV4.y, colorV4.z, colorV4.w);

                comp->intensity      = ReadOptionalFloat(L, tableIndex, "intensity", 1.0f);
                comp->range          = ReadOptionalFloat(L, tableIndex, "range", 10.0f);
                comp->spotAngle      = ReadOptionalFloat(L, tableIndex, "spotAngle", 45.0f);
                comp->spotInnerAngle = ReadOptionalFloat(L, tableIndex, "spotInnerAngle", 30.0f);
                comp->syncPosition   = ReadOptionalBool(L, tableIndex, "syncPosition", true);
                comp->syncDirection  = ReadOptionalBool(L, tableIndex, "syncDirection", true);

                comp->SyncProperties();
            }

            void ConfigureAudioSource(lua_State* L, int tableIndex, ECS::AudioSourceComponent* comp) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

                const std::string clipPath = ReadOptionalString(L, tableIndex, "clip", "");
                if (!clipPath.empty()) {
                    Audio::AudioClip* clip = resources.LoadAudioClip(clipPath);
                    if (clip) {
                        comp->SetClip(clip);
                    }
                }

                comp->SetVolume(ReadOptionalFloat(L, tableIndex, "volume", 1.0f));
                comp->SetPitch(ReadOptionalFloat(L, tableIndex, "pitch", 1.0f));
                comp->SetLoop(ReadOptionalBool(L, tableIndex, "loop", false));
                comp->SetPlayOnStart(ReadOptionalBool(L, tableIndex, "playOnStart", false));
            }

            void ConfigureRigidBody(lua_State* L, int tableIndex, ECS::RigidBodyComponent* comp, ECS::GameObject* /*gameObject*/) {
                auto rigidBody = std::make_unique<Physics::RigidBody>();

                const std::string bodyType = ReadOptionalString(L, tableIndex, "bodyType", "Dynamic");
                if (bodyType == "Static") {
                    rigidBody->SetType(Physics::RigidBodyType::Static);
                }
                else if (bodyType == "Dynamic") {
                    rigidBody->SetType(Physics::RigidBodyType::Dynamic);
                }
                else if (bodyType == "Kinematic") {
                    rigidBody->SetType(Physics::RigidBodyType::Kinematic);
                }

                rigidBody->SetMass(ReadOptionalFloat(L, tableIndex, "mass", 1.0f));
                rigidBody->SetFriction(ReadOptionalFloat(L, tableIndex, "friction", 0.5f));
                rigidBody->SetRestitution(ReadOptionalFloat(L, tableIndex, "restitution", 0.0f));

                comp->SetRigidBody(std::move(rigidBody));
            }

            void ConfigureBoxCollider(lua_State* L, int tableIndex, ECS::BoxColliderComponent* comp, ECS::GameObject* gameObject) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

                const std::string colliderMesh = ReadOptionalString(L, tableIndex, "mesh", "");
                if (!colliderMesh.empty()) {
                    Rendering::Mesh* mesh = resources.LoadModel(colliderMesh);
                    if (mesh) {
                        Physics::BoxCollider tempCollider(mesh);
                        const Math::Vector3 size = tempCollider.GetSize() * gameObject->GetTransform().GetScale();
                        comp->SetSize(size);
                    }
                }
                else {
                    const Math::Vector3 size = ReadOptionalVector3(L, tableIndex, "size", Math::Vector3(1.0f, 1.0f, 1.0f));
                    comp->SetSize(size * gameObject->GetTransform().GetScale());
                }

                comp->SetIsTrigger(ReadOptionalBool(L, tableIndex, "isTrigger", false));
            }

            void ConfigureSphereCollider(lua_State* L, int tableIndex, ECS::SphereColliderComponent* comp, ECS::GameObject* gameObject) {
                const Math::Vector3 scale = gameObject->GetTransform().GetScale();
                const float radius = ReadOptionalFloat(L, tableIndex, "radius", 0.5f) *
                    std::max(std::abs(scale.x), std::abs(scale.z));
                const Math::Vector3 centerOffset = ReadOptionalVector3(L, tableIndex, "centerOffset", Math::Vector3()) * scale;

                comp->SetRadius(radius);
                comp->SetCenterOffset(centerOffset);
                comp->SetIsTrigger(ReadOptionalBool(L, tableIndex, "isTrigger", false));
            }

            void ConfigureCapsuleCollider(lua_State* L, int tableIndex, ECS::CapsuleColliderComponent* comp, ECS::GameObject* gameObject) {
                const Math::Vector3 scale = gameObject->GetTransform().GetScale();
                const float radius = ReadOptionalFloat(L, tableIndex, "radius", 0.35f) *
                    std::max(std::abs(scale.x), std::abs(scale.z));
                const float height = ReadOptionalFloat(L, tableIndex, "height", 1.8f) * std::abs(scale.y);
                const Math::Vector3 centerOffset = ReadOptionalVector3(L, tableIndex, "centerOffset", Math::Vector3(0.0f, 0.9f, 0.0f)) * scale;

                comp->SetRadius(radius);
                comp->SetHeight(height);
                comp->SetCenterOffset(centerOffset);
                comp->SetIsTrigger(ReadOptionalBool(L, tableIndex, "isTrigger", false));
            }

            void ConfigureCameraComponent(lua_State* L, int tableIndex, ECS::CameraComponent* comp) {
                comp->fov               = ReadOptionalFloat(L, tableIndex, "fov", 45.0f);
                comp->nearClip          = ReadOptionalFloat(L, tableIndex, "nearClip", 0.1f);
                comp->farClip           = ReadOptionalFloat(L, tableIndex, "farClip", 1000.0f);
                comp->orthographicSize  = ReadOptionalFloat(L, tableIndex, "orthographicSize", 10.0f);
                comp->syncWithTransform = ReadOptionalBool(L, tableIndex, "syncWithTransform", true);
                comp->isMainCamera      = ReadOptionalBool(L, tableIndex, "isMainCamera", false);

                const int projInt = ReadOptionalInt(L, tableIndex, "projectionType", 0);
                comp->projectionType = static_cast<Rendering::ProjectionType>(projInt);

                comp->SetFOV(comp->fov);
                comp->SetNearPlane(comp->nearClip);
                comp->SetFarPlane(comp->farClip);
                comp->SetProjectionType(comp->projectionType);
                comp->SetOrthographicSize(comp->orthographicSize);

                comp->OnValidate();
            }

            void ConfigureFreeLookCamera(lua_State* L, int tableIndex, ECS::FreeLookCamera* comp) {
                comp->SetMoveSpeed(ReadOptionalFloat(L, tableIndex, "moveSpeed", 5.0f));
                comp->SetLookSpeed(ReadOptionalFloat(L, tableIndex, "lookSpeed", 0.1f));
                comp->SetRotationSpeed(ReadOptionalFloat(L, tableIndex, "rotationSpeed", 90.0f));
            }

            void ConfigureAnimator(lua_State* L, int tableIndex, Animation::Animator* comp) {
                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

                std::string modelPath = ReadOptionalString(L, tableIndex, "modelRef", "");
                if (modelPath.empty()) modelPath = ReadOptionalString(L, tableIndex, "model", "");
                comp->modelRef = modelPath;

                if (!modelPath.empty()) {
                    Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(modelPath);

                    if (modelData.skeleton) {
                        comp->SetSkeleton(modelData.skeleton);
                    }

                    if (!modelData.meshes.empty()) {
                        resources.RegisterMeshes(modelPath, modelData.meshes);
                        comp->SetMeshes(modelData.meshes);
                    }

                    for (const auto& clip : modelData.animations) {
                        comp->AddClip(clip->GetName(), clip);
                    }
                }

                // Load additional animation-only FBX files (clips merged, meshes ignored)
                comp->additionalModels.clear();
                lua_getfield(L, tableIndex, "additionalModels");
                if (lua_istable(L, -1)) {
                    int additionalTable = lua_gettop(L);
                    lua_pushnil(L);
                    while (lua_next(L, additionalTable) != 0) {
                        if (lua_isstring(L, -1)) {
                            std::string addPath = lua_tostring(L, -1);
                            comp->additionalModels.push_back(addPath);

                            Rendering::ModelData addData = Rendering::ModelLoader::LoadModelWithAnimations(addPath);
                            if (addData.animations.empty() && addData.meshes.empty()) {
                                RTB_WARN("[Animator] Additional model not found or empty: " + addPath);
                            } else {
                                for (const auto& clip : addData.animations) {
                                    comp->AddClip(clip->GetName(), clip);
                                }
                            }
                            // Free meshes from additional models — only clips are kept
                            for (Rendering::Mesh* mesh : addData.meshes) {
                                delete mesh;
                            }
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);

                // Strip legacy "vendor|" prefix from clip name fields (e.g. old scene files with "mixamo.com|Walk")
                auto StripPrefix = [](const std::string& s) -> std::string {
                    size_t pipe = s.find('|');
                    return (pipe != std::string::npos) ? s.substr(pipe + 1) : s;
                };

                // Sync defaultClip from Lua
                comp->defaultClip = StripPrefix(ReadOptionalString(L, tableIndex, "defaultClip", ""));

                std::string clipName = ReadOptionalString(L, tableIndex, "currentClipName", "");
                if (clipName.empty()) clipName = ReadOptionalString(L, tableIndex, "defaultClip", "");
                clipName = StripPrefix(clipName);
                bool loop = ReadOptionalBool(L, tableIndex, "looping", true);
                if (!loop) loop = ReadOptionalBool(L, tableIndex, "loop", true);
                const bool shouldPlay = ReadOptionalBool(L, tableIndex, "playing", false);
                comp->SetSpeed(ReadOptionalFloat(L, tableIndex, "speed", 1.0f));
                comp->looping = loop;
                comp->playing = false;

                if (!clipName.empty() && comp->GetClip(clipName) != nullptr) {
                    if (shouldPlay) {
                        comp->Play(clipName, loop);
                    } else {
                        comp->SelectClip(clipName, loop);
                    }
                } else {
                    auto clipNames = comp->GetClipNames();
                    if (!clipNames.empty()) {
                        if (shouldPlay) {
                            comp->Play(clipNames[0], loop);
                        } else {
                            comp->SelectClip(clipNames[0], loop);
                        }
                    }
                }

                comp->playing = shouldPlay;
            }

        }
    }
}

