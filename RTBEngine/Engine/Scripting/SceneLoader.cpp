#include "../ECS/NavGridComponent.h"
#include "SceneLoader.h"
#include "ComponentRegistry.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/Component.h"
#include "../ECS/MissingComponent.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/SphereColliderComponent.h"
#include "../ECS/CapsuleColliderComponent.h"
#include "../ECS/Prefab.h"
#include "../ECS/PrefabRegistry.h"
#include "../Core/ResourceManager.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Rendering/Lighting/PointLight.h"
#include "../Rendering/Lighting/SpotLight.h"
#include "../Rendering/ModelLoader.h"
#include "../Physics/RigidBody.h"
#include "../Physics/BoxCollider.h"
#include "../Physics/SphereCollider.h"
#include "../Physics/CapsuleCollider.h"
#include "../Physics/PhysicsLayerSettings.h"
#include "../Math/Math.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UIContainer.h"
#include "../UI/Elements/UIHorizontalLayout.h"
#include "../UI/Elements/UIVerticalLayout.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"
#include "../Animation/Animator.h"
#include "../Reflection/TypeInfo.h"
#include "../Rendering/FbxBinding.h"

#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <cstdio>
#include "../RTBEngine.h"

#include "SceneLuaBindings.h"
#include "SceneComponentConfigurator.h"
#include "SceneParsingUtils.h"
#include "SceneReflectionUtils.h"

namespace RTBEngine {
    namespace Scripting {

        namespace {
            void ReadCollisionLayer(lua_State* L, int tableIndex, ECS::GameObject* gameObject)
            {
                if (!gameObject) {
                    return;
                }

                lua_getfield(L, tableIndex, "collisionLayer");
                if (lua_isstring(L, -1)) {
                    gameObject->SetCollisionLayerByName(lua_tostring(L, -1));
                } else if (lua_isnumber(L, -1)) {
                    gameObject->SetCollisionLayer(static_cast<int>(lua_tointeger(L, -1)));
                }
                lua_pop(L, 1);
            }
        }

        void SceneLoader::SetupLuaBindings(lua_State* L) {
            SceneLuaBindings::SetupLuaBindings(L);
        }

        ECS::Scene* SceneLoader::LoadScene(const std::string& filePath) {
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);
            SetupLuaBindings(L);
            const std::string resolvedFilePath = Core::ResourceManager::GetInstance().ResolvePathForRead(filePath);

            if (luaL_dofile(L, resolvedFilePath.c_str()) != LUA_OK) {
                RTB_ERROR("SceneLoader: Failed to load file '" + filePath + "': " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            lua_getglobal(L, "CreateScene");
            if (!lua_isfunction(L, -1)) {
                RTB_ERROR("SceneLoader: 'CreateScene' function not found in " + filePath);
                lua_close(L);
                return nullptr;
            }

            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                RTB_ERROR("SceneLoader: Error calling CreateScene(): " + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            if (!SceneParsingUtils::ValidateSceneTable(L, -1, filePath)) {
                lua_close(L);
                return nullptr;
            }

            const std::string sceneName = SceneParsingUtils::ReadOptionalString(L, -1, "name", "Unnamed Scene");
            ECS::Scene* scene = new ECS::Scene(sceneName);

            std::string skyboxPath = SceneParsingUtils::ReadOptionalString(L, -1, "skybox", "");
            bool skyboxEnabled = SceneParsingUtils::ReadOptionalBool(L, -1, "skyboxEnabled", true);
            scene->SetSkyboxEnabled(skyboxEnabled);
            if (!skyboxPath.empty()) {
                Rendering::Cubemap* cubemap = Core::ResourceManager::GetInstance().LoadCubemapAsset(skyboxPath);
                if (cubemap)
                    scene->SetSkyboxCubemap(cubemap);
            }

            std::vector<std::pair<ECS::GameObject*, std::string>> parentingRequests;
            std::vector<UUIDRefRequest> uuidRefRequests;

            lua_getfield(L, -1, "gameObjects");
            if (lua_istable(L, -1)) {
                int count = luaL_len(L, -1);
                for (int i = 1; i <= count; i++) {
                    lua_geti(L, -1, i);
                    if (lua_istable(L, -1)) {
                        ECS::GameObject* go = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
                        if (go)
                            scene->AddGameObject(go);
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);

            ResolveParenting(scene, parentingRequests);
            ECS::NavGridComponent::FinalizeImportsForScene(scene);
            ResolveUUIDRefs(scene, uuidRefRequests);

            lua_close(L);
            return scene;
        }

        ECS::GameObject* SceneLoader::ProcessGameObject(lua_State* L, int tableIndex, ECS::Scene* scene,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            lua_getfield(L, tableIndex, "name");
            std::string name = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Unnamed";
            lua_pop(L, 1);

            std::string savedUUID = SceneParsingUtils::ReadOptionalString(L, tableIndex, "uuid", "");
            std::string prefabName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "prefab", "");

            if (!prefabName.empty() && ECS::PrefabRegistry::GetInstance().Has(prefabName)) {
                return ProcessPrefabInstance(L, tableIndex, scene, name, savedUUID, parentingRequests, uuidRefRequests);
            }

            if (!prefabName.empty()) {
                RTB_WARN("SceneLoader: Prefab '" + prefabName + "' not found for '" + name + "' — loading as plain GameObject");
            }

            ECS::GameObject* go = new ECS::GameObject(name);
            if (!savedUUID.empty())
                go->SetUUID(savedUUID);

            go->SetActive(SceneParsingUtils::ReadOptionalBool(L, tableIndex, "active", true));

            ReadTransform(L, tableIndex, go);
            ReadCollisionLayer(L, tableIndex, go);

            std::string parentName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "parent", "");
            if (!parentName.empty())
                parentingRequests.push_back({ go, parentName });

            lua_getfield(L, tableIndex, "components");
            if (lua_istable(L, -1))
                ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
            lua_pop(L, 1);

            ProcessChildren(L, tableIndex, scene, go, parentingRequests, uuidRefRequests);

            return go;
        }

        ECS::GameObject* SceneLoader::FindDirectChild(ECS::GameObject* parent,
            const std::string& uuid, const std::string& name)
        {
            if (!parent) {
                return nullptr;
            }

            if (!uuid.empty()) {
                for (ECS::GameObject* child : parent->GetChildren()) {
                    if (child && child->GetUUID() == uuid) {
                        return child;
                    }
                }
            }

            if (!name.empty()) {
                for (ECS::GameObject* child : parent->GetChildren()) {
                    if (child && child->GetName() == name) {
                        return child;
                    }
                }
            }

            return nullptr;
        }

        void SceneLoader::ApplySceneGameObjectFromTable(lua_State* L, int tableIndex, ECS::Scene* scene,
            ECS::GameObject* go,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            if (!go) {
                return;
            }

            go->SetActive(SceneParsingUtils::ReadOptionalBool(L, tableIndex, "active", go->IsActive()));
            ReadTransform(L, tableIndex, go);
            ReadCollisionLayer(L, tableIndex, go);

            lua_getfield(L, tableIndex, "overrides");
            if (lua_istable(L, -1)) {
                int overridesIndex = lua_gettop(L);
                lua_getfield(L, overridesIndex, "components");
                if (lua_istable(L, -1)) {
                    ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            lua_getfield(L, tableIndex, "components");
            if (lua_istable(L, -1)) {
                ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
            }
            lua_pop(L, 1);

            MergeSceneChildren(L, tableIndex, scene, go, parentingRequests, uuidRefRequests);
        }

        void SceneLoader::MergeSceneChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
            ECS::GameObject* parent,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            if (!parent) {
                return;
            }

            lua_getfield(L, tableIndex, "children");
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                return;
            }

            const int childrenTableIndex = lua_gettop(L);
            const int count = luaL_len(L, childrenTableIndex);
            for (int c = 1; c <= count; ++c) {
                lua_geti(L, childrenTableIndex, c);
                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    continue;
                }

                const int childTableIndex = lua_gettop(L);
                const std::string childUuid = SceneParsingUtils::ReadOptionalString(L, childTableIndex, "uuid", "");
                const std::string childName = SceneParsingUtils::ReadOptionalString(L, childTableIndex, "name", "");

                ECS::GameObject* existing = nullptr;
                if (!childUuid.empty()) {
                    for (ECS::GameObject* child : parent->GetChildren()) {
                        if (child && child->GetUUID() == childUuid) {
                            existing = child;
                            break;
                        }
                    }
                }

                if (!existing && !childName.empty()) {
                    existing = FindDirectChild(parent, "", childName);
                }

                if (existing) {
                    if (!childUuid.empty() && existing->GetUUID() != childUuid) {
                        existing->SetUUID(childUuid);
                    }
                    ApplySceneGameObjectFromTable(L, childTableIndex, scene, existing, parentingRequests, uuidRefRequests);
                } else {
                    ECS::GameObject* child = ProcessGameObject(L, childTableIndex, scene, parentingRequests, uuidRefRequests);
                    if (child) {
                        scene->AddGameObject(child);
                        child->SetParent(parent);
                    }
                }

                lua_pop(L, 1);
            }

            lua_pop(L, 1);
        }

        ECS::GameObject* SceneLoader::ProcessPrefabInstance(lua_State* L, int tableIndex, ECS::Scene* scene,
            const std::string& name, const std::string& uuid,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            std::string prefabName = SceneParsingUtils::ReadOptionalString(L, tableIndex, "prefab", "");
            const ECS::Prefab* prefab = ECS::PrefabRegistry::GetInstance().Get(prefabName);
            if (!prefab) {
                RTB_WARN("SceneLoader: Prefab '" + prefabName + "' not found — creating empty GO");
                ECS::GameObject* go = new ECS::GameObject(name);
                if (!uuid.empty()) {
                    go->SetUUID(uuid);
                }
                ApplySceneGameObjectFromTable(L, tableIndex, scene, go, parentingRequests, uuidRefRequests);
                return go;
            }

            std::vector<ECS::GameObject*> childGOs;
            ECS::GameObject* go = prefab->Instantiate(nullptr, childGOs);
            go->SetName(name);
            if (!uuid.empty())
                go->SetUUID(uuid);

            go->SetActive(SceneParsingUtils::ReadOptionalBool(L, tableIndex, "active", true));

            for (ECS::GameObject* child : childGOs) {
                if (child) {
                    scene->AddGameObject(child);
                }
            }

            ReadTransform(L, tableIndex, go);

            lua_getfield(L, tableIndex, "overrides");
            if (lua_istable(L, -1)) {
                int overridesIndex = lua_gettop(L);
                lua_getfield(L, overridesIndex, "components");
                if (lua_istable(L, -1))
                    ProcessComponents(L, lua_gettop(L), go, uuidRefRequests);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            MergeSceneChildren(L, tableIndex, scene, go, parentingRequests, uuidRefRequests);

            return go;
        }

        void SceneLoader::ReadTransform(lua_State* L, int tableIndex, ECS::GameObject* go) {
            lua_getfield(L, tableIndex, "position");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) go->GetTransform().SetPosition(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, tableIndex, "rotation");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Quaternion>::get(L, -1);
                if (result) go->GetTransform().SetRotation(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, tableIndex, "scale");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) go->GetTransform().SetScale(result.value());
            }
            lua_pop(L, 1);
        }

        void SceneLoader::ProcessChildren(lua_State* L, int tableIndex, ECS::Scene* scene,
            ECS::GameObject* parent,
            std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            lua_getfield(L, tableIndex, "children");
            if (lua_istable(L, -1)) {
                int count = luaL_len(L, -1);
                for (int c = 1; c <= count; c++) {
                    lua_geti(L, -1, c);
                    if (lua_istable(L, -1)) {
                        ECS::GameObject* child = ProcessGameObject(L, lua_gettop(L), scene, parentingRequests, uuidRefRequests);
                        if (child) {
                            scene->AddGameObject(child);
                            child->SetParent(parent);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }

        void SceneLoader::ResolveParenting(ECS::Scene* scene,
            const std::vector<std::pair<ECS::GameObject*, std::string>>& parentingRequests)
        {
            for (const auto& req : parentingRequests) {
                ECS::GameObject* parent = scene->FindGameObject(req.second);
                if (parent)
                    req.first->SetParent(parent);
                else
                    RTB_WARN("SceneLoader: Parent '" + req.second + "' not found for '" + req.first->GetName() + "'");
            }
        }

        void SceneLoader::ResolveUUIDRefs(ECS::Scene* scene,
            const std::vector<RTBEngine::Scripting::SceneLoader::UUIDRefRequest>& uuidRefRequests)
        {
            for (const auto& req : uuidRefRequests) {
                const std::string& uuidStr = req.uuidString;

                if (req.prop->type == Reflection::PropertyType::GameObjectRef) {
                    ECS::GameObject* target = scene->FindGameObjectByUUID(uuidStr);
                    if (target) {
                        void* data = req.prop->GetMutableData(req.component);
                        *(ECS::GameObject**)data = target;
                    } else {
                        RTB_WARN("SceneLoader: GameObjectRef UUID not found: " + uuidStr);
                    }
                }
                else if (req.prop->type == Reflection::PropertyType::ComponentRef) {
                    size_t slash = uuidStr.find('/');
                    if (slash != std::string::npos) {
                        std::string uuid = uuidStr.substr(0, slash);
                        std::string type = uuidStr.substr(slash + 1);
                        ECS::GameObject* targetGO = scene->FindGameObjectByUUID(uuid);
                        if (targetGO) {
                            for (const auto& comp : targetGO->GetComponents()) {
                                if (std::string(comp->GetTypeName()) == type) {
                                    void* data = req.prop->GetMutableData(req.component);
                                    *(ECS::Component**)data = comp.get();
                                    break;
                                }
                            }
                        } else {
                            RTB_WARN("SceneLoader: ComponentRef UUID not found: " + uuid);
                        }
                    }
                }
            }
        }

        void SceneLoader::ProcessComponents(lua_State* L, int arrayIndex, ECS::GameObject* gameObject,
            std::vector<UUIDRefRequest>& uuidRefRequests)
        {
            int componentCount = luaL_len(L, arrayIndex);

            for (int i = 1; i <= componentCount; i++) {
                lua_geti(L, arrayIndex, i);
                int componentTableIndex = lua_gettop(L);

                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    continue;
                }

                lua_getfield(L, -1, "type");
                if (!lua_isstring(L, -1)) {
                    lua_pop(L, 2);
                    continue;
                }

                std::string componentType = lua_tostring(L, -1);
                lua_pop(L, 1);

                const Reflection::TypeInfo* registeredTypeInfo =
                    ComponentRegistry::GetInstance().GetComponentTypeInfo(componentType);

                ECS::Component* comp = ComponentRegistry::GetInstance().CreateComponent(componentType);
                if (!comp) {
                    RTB_ERROR("SceneLoader: Component type '" + componentType + "' not found — inserting MissingComponent placeholder");
                    gameObject->AddComponent(new ECS::MissingComponent(componentType));
                    lua_pop(L, 1);
                    continue;
                }

                ECS::Component* existing = nullptr;
                for (const auto& c : gameObject->GetComponents()) {
                    if (std::string(c->GetTypeName()) == componentType) {
                        existing = c.get();
                        break;
                    }
                }

                if (existing) {
                    ComponentRegistry::GetInstance().DestroyComponent(componentType, comp);
                    comp = existing;
                } else {
                    gameObject->AddComponent(comp, registeredTypeInfo);
                }

                SceneReflectionUtils::ApplyLuaTableToComponent(L, componentTableIndex, componentType.c_str(), comp);
                SceneReflectionUtils::ClearReferenceProperties(comp, registeredTypeInfo);

                if (componentType == "MeshRenderer")
                    SceneComponentConfigurator::ConfigureMeshRenderer(L, componentTableIndex, static_cast<ECS::MeshRenderer*>(comp));
                else if (componentType == "LightComponent")
                    SceneComponentConfigurator::ConfigureLightComponent(L, componentTableIndex, static_cast<ECS::LightComponent*>(comp));
                else if (componentType == "AudioSourceComponent")
                    SceneComponentConfigurator::ConfigureAudioSource(L, componentTableIndex, static_cast<ECS::AudioSourceComponent*>(comp));
                else if (componentType == "RigidBodyComponent")
                    SceneComponentConfigurator::ConfigureRigidBody(L, componentTableIndex, static_cast<ECS::RigidBodyComponent*>(comp), gameObject);
                else if (componentType == "BoxColliderComponent")
                    SceneComponentConfigurator::ConfigureBoxCollider(L, componentTableIndex, static_cast<ECS::BoxColliderComponent*>(comp), gameObject);
                else if (componentType == "SphereColliderComponent")
                    SceneComponentConfigurator::ConfigureSphereCollider(L, componentTableIndex, static_cast<ECS::SphereColliderComponent*>(comp), gameObject);
                else if (componentType == "CapsuleColliderComponent")
                    SceneComponentConfigurator::ConfigureCapsuleCollider(L, componentTableIndex, static_cast<ECS::CapsuleColliderComponent*>(comp), gameObject);
                else if (componentType == "Canvas")
                    SceneComponentConfigurator::ConfigureCanvas(L, componentTableIndex, static_cast<UI::Canvas*>(comp));
                else if (componentType == "UIText")
                    SceneComponentConfigurator::ConfigureUIText(L, componentTableIndex, static_cast<UI::UIText*>(comp));
                else if (componentType == "UIImage")
                    SceneComponentConfigurator::ConfigureUIImage(L, componentTableIndex, static_cast<UI::UIImage*>(comp));
                else if (componentType == "UIPanel")
                    SceneComponentConfigurator::ConfigureUIPanel(L, componentTableIndex, static_cast<UI::UIPanel*>(comp));
                else if (componentType == "UIButton")
                    SceneComponentConfigurator::ConfigureUIButton(L, componentTableIndex, static_cast<UI::UIButton*>(comp));
                else if (componentType == "UIContainer")
                    SceneComponentConfigurator::SyncUIElementProxies(L, componentTableIndex, static_cast<UI::UIElement*>(comp));
                else if (componentType == "UIHorizontalLayout")
                    SceneComponentConfigurator::ConfigureUILayout(L, componentTableIndex, static_cast<UI::UIHorizontalLayout*>(comp));
                else if (componentType == "UIVerticalLayout")
                    SceneComponentConfigurator::ConfigureUILayout(L, componentTableIndex, static_cast<UI::UIVerticalLayout*>(comp));
                else if (componentType == "CameraComponent")
                    SceneComponentConfigurator::ConfigureCameraComponent(L, componentTableIndex, static_cast<ECS::CameraComponent*>(comp));
                else if (componentType == "FreeLookCamera")
                    SceneComponentConfigurator::ConfigureFreeLookCamera(L, componentTableIndex, static_cast<ECS::FreeLookCamera*>(comp));
                else if (componentType == "Animator")
                    SceneComponentConfigurator::ConfigureAnimator(L, componentTableIndex, static_cast<Animation::Animator*>(comp));
                else if (componentType == "NavGridComponent")
                    SceneComponentConfigurator::ConfigureNavGrid(L, componentTableIndex, static_cast<ECS::NavGridComponent*>(comp));

                const Reflection::TypeInfo* typeInfo = registeredTypeInfo ? registeredTypeInfo : comp->GetTypeInfo();
                if (typeInfo) {
                    for (const auto* prop : typeInfo->GetSerializableProperties()) {
                        if (!prop) {
                            continue;
                        }
                        if (prop->type == Reflection::PropertyType::GameObjectRef ||
                            prop->type == Reflection::PropertyType::ComponentRef)
                        {
                            lua_getfield(L, componentTableIndex, prop->name.c_str());
                            if (lua_isstring(L, -1)) {
                                std::string uuidStr = lua_tostring(L, -1);
                                if (!uuidStr.empty())
                                    uuidRefRequests.push_back({ comp, prop, uuidStr });
                            }
                            lua_pop(L, 1);
                        }
                    }
                }

                lua_pop(L, 1);
            }
        }

        namespace {
            bool ChildUsesAnimatorModel(ECS::GameObject* child, const std::string& animatorModelPath)
            {
                if (!child) {
                    return false;
                }

                auto* renderer = child->GetComponent<ECS::MeshRenderer>();
                if (!renderer || !renderer->meshRef) {
                    return false;
                }

                Core::ResourceManager& resources = Core::ResourceManager::GetInstance();
                const std::string meshPath = resources.GetMeshPath(renderer->meshRef);
                if (meshPath.empty() || animatorModelPath.empty()) {
                    return false;
                }

                return resources.ResolvePathForRead(meshPath) ==
                    resources.ResolvePathForRead(animatorModelPath);
            }
        }

        void SceneLoader::RebuildFbxHierarchies(ECS::Scene* scene)
        {
            if (!scene) return;

            Core::ResourceManager& resources = Core::ResourceManager::GetInstance();

            // Two passes:
            // 1) Multi-mesh FBX: rebuild child GO hierarchy from FBX node tree
            // 2) All Animators with skeletons: create bone GOs (OnValidate couldn't because scene wasn't active yet)

            struct RebuildEntry {
                ECS::GameObject* go;
                Animation::Animator* animator;
                Rendering::ModelData modelData;
            };
            std::vector<RebuildEntry> multiMeshRebuild;

            for (const auto& go : scene->GetGameObjects()) {
                auto* animator = go->GetComponent<Animation::Animator>();
                if (!animator || animator->modelRef.empty()) continue;

                // Skip auto-rebuild when the scene already defines character mesh parts.
                bool hasMeshChildren = false;
                for (const auto* child : go->GetChildren()) {
                    if (!child) {
                        continue;
                    }
                    if (ChildUsesAnimatorModel(const_cast<ECS::GameObject*>(child), animator->modelRef)) {
                        hasMeshChildren = true;
                        break;
                    }
                }
                if (hasMeshChildren) continue;

                Rendering::ModelData modelData = Rendering::ModelLoader::LoadModelWithAnimations(animator->modelRef);
                if (!modelData.meshes.empty()) {
                    multiMeshRebuild.push_back({ go.get(), animator, std::move(modelData) });
                }
            }

            // Pass 1: Multi-mesh FBX hierarchy rebuild
            for (auto& entry : multiMeshRebuild) {
                ECS::GameObject* go = entry.go;
                Animation::Animator* animator = entry.animator;
                Rendering::ModelData& modelData = entry.modelData;

                resources.RegisterMeshes(animator->modelRef, modelData.meshes);

                Rendering::FbxBindingContext ctx{ resources, animator->modelRef, modelData };
                Rendering::FbxBindingResult binding = Rendering::BuildMeshesAndMaterials(ctx);
                Rendering::Shader* basicShader = resources.GetShader("basic");

                auto* rootRenderer = go->GetComponent<ECS::MeshRenderer>();
                if (rootRenderer) go->RemoveComponent(rootRenderer);

                Rendering::AttachFbxMeshesToHierarchy(scene, go, modelData, binding, basicShader);
            }

            // Pass 2: Create bone GameObjects for all Animators with skeletons
            // OnValidate couldn't do this because the scene wasn't active in SceneManager yet
            // Collect first to avoid invalidating the GO list while iterating
            std::vector<Animation::Animator*> animatorsNeedingBones;
            for (const auto& go : scene->GetGameObjects()) {
                auto* animator = go->GetComponent<Animation::Animator>();
                if (!animator) continue;
                if (animator->AreBoneGOsCreated()) continue;
                if (!animator->HasBones()) continue;
                animatorsNeedingBones.push_back(animator);
            }
            for (auto* animator : animatorsNeedingBones) {
                animator->CreateBoneGameObjects(scene);
            }
        }

    }
}
