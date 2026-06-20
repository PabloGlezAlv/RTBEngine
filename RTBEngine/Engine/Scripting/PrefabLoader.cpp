#include "PrefabLoader.h"
#include "../Scene/Prefab.h"
#include "../Scene/Component.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/SceneComponentConfigurator.h"
#include "../Scripting/SceneReflectionUtils.h"
#include "../Scripting/SceneLuaBindings.h"
#include "../Scene/MeshRenderer.h"
#include "../Scene/LightComponent.h"
#include "../Scene/AudioSourceComponent.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Scene/CameraComponent.h"
#include "../Scene/FreeLookCamera.h"
#include "../Animation/Animator.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UIContainer.h"
#include "../UI/Elements/UIHorizontalLayout.h"
#include "../UI/Elements/UIVerticalLayout.h"
#include "../Physics/PhysicsLayerSettings.h"
#include "../RTBEngine.h"
#include "../Reflection/TypeInfo.h"
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

namespace RTBEngine {
    namespace Scripting {

        static void CaptureLuaReferenceProperties(lua_State* L,
            int compTableIndex,
            ECS::Component* comp,
            ECS::ComponentSnapshot& snap)
        {
            const char* typeName = comp->GetTypeName();
            if (!typeName || typeName[0] == '\0') {
                return;
            }

            const Reflection::TypeInfo* typeInfo =
                Reflection::TypeRegistry::GetInstance().GetTypeInfo(typeName);
            if (!typeInfo) {
                return;
            }

            for (const Reflection::PropertyInfo* prop : typeInfo->GetSerializableProperties()) {
                if (!prop ||
                    (prop->type != Reflection::PropertyType::GameObjectRef &&
                     prop->type != Reflection::PropertyType::ComponentRef)) {
                    continue;
                }

                lua_getfield(L, compTableIndex, prop->name.c_str());
                if (lua_isstring(L, -1)) {
                    const char* refValue = lua_tostring(L, -1);
                    if (refValue && refValue[0] != '\0') {
                        snap.ptrPathData[prop->offset] = refValue;
                    }
                }
                lua_pop(L, 1);
            }
        }

        //Internal helpers
        static void LoadComponents(lua_State* L, int nodeTableIndex, ECS::Prefab& prefab)
        {
            lua_getfield(L, nodeTableIndex, "components");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return;
            }

            int componentCount = static_cast<int>(luaL_len(L, -1));
            int componentsTableIndex = lua_gettop(L);

            for (int i = 1; i <= componentCount; i++)
            {
                lua_geti(L, componentsTableIndex, i);
                int compTableIndex = lua_gettop(L);

                if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }

                lua_getfield(L, compTableIndex, "type");
                if (!lua_isstring(L, -1)) { lua_pop(L, 2); continue; }

                std::string typeName = lua_tostring(L, -1);
                lua_pop(L, 1);

                ECS::Component* comp = Scripting::ComponentRegistry::GetInstance().CreateComponent(typeName);
                if (!comp) { lua_pop(L, 1); continue; }

                Scripting::SceneReflectionUtils::ApplyLuaTableToComponent(L, compTableIndex, typeName.c_str(), comp);

                if (typeName == "MeshRenderer")
                    Scripting::SceneComponentConfigurator::ConfigureMeshRenderer(L, compTableIndex, static_cast<ECS::MeshRenderer*>(comp));
                else if (typeName == "LightComponent")
                    Scripting::SceneComponentConfigurator::ConfigureLightComponent(L, compTableIndex, static_cast<ECS::LightComponent*>(comp));
                else if (typeName == "AudioSourceComponent")
                    Scripting::SceneComponentConfigurator::ConfigureAudioSource(L, compTableIndex, static_cast<ECS::AudioSourceComponent*>(comp));
                else if (typeName == "CameraComponent")
                    Scripting::SceneComponentConfigurator::ConfigureCameraComponent(L, compTableIndex, static_cast<ECS::CameraComponent*>(comp));
                else if (typeName == "FreeLookCamera")
                    Scripting::SceneComponentConfigurator::ConfigureFreeLookCamera(L, compTableIndex, static_cast<ECS::FreeLookCamera*>(comp));
                else if (typeName == "Animator")
                    Scripting::SceneComponentConfigurator::ConfigureAnimator(L, compTableIndex, static_cast<Animation::Animator*>(comp));
                else if (typeName == "Canvas")
                    Scripting::SceneComponentConfigurator::ConfigureCanvas(L, compTableIndex, static_cast<UI::Canvas*>(comp));
                else if (typeName == "UIText")
                    Scripting::SceneComponentConfigurator::ConfigureUIText(L, compTableIndex, static_cast<UI::UIText*>(comp));
                else if (typeName == "UIImage")
                    Scripting::SceneComponentConfigurator::ConfigureUIImage(L, compTableIndex, static_cast<UI::UIImage*>(comp));
                else if (typeName == "UIPanel")
                    Scripting::SceneComponentConfigurator::ConfigureUIPanel(L, compTableIndex, static_cast<UI::UIPanel*>(comp));
                else if (typeName == "UIButton")
                    Scripting::SceneComponentConfigurator::ConfigureUIButton(L, compTableIndex, static_cast<UI::UIButton*>(comp));
                else if (typeName == "UIHorizontalLayout")
                    Scripting::SceneComponentConfigurator::ConfigureUILayout(L, compTableIndex, static_cast<UI::UIHorizontalLayout*>(comp));
                else if (typeName == "UIVerticalLayout")
                    Scripting::SceneComponentConfigurator::ConfigureUILayout(L, compTableIndex, static_cast<UI::UIVerticalLayout*>(comp));

                comp->OnValidate();

                ECS::ComponentSnapshot snap;
                ECS::Prefab::SnapshotComponent(snap, comp);
                CaptureLuaReferenceProperties(L, compTableIndex, comp, snap);
                prefab.AddSnapshot(std::move(snap));

                ComponentRegistry::GetInstance().DestroyComponent(typeName, comp);
                lua_pop(L, 1);
            }

            // pop components table
            lua_pop(L, 1);
        }

        static void LoadCollisionLayer(lua_State* L, int nodeTableIndex, ECS::Prefab& prefab)
        {
            lua_getfield(L, nodeTableIndex, "collisionLayer");
            if (lua_isstring(L, -1)) {
                prefab.SetCollisionLayer(Physics::PhysicsLayerSettings::Get().GetLayerIndex(lua_tostring(L, -1)));
            } else if (lua_isnumber(L, -1)) {
                prefab.SetCollisionLayer(static_cast<int>(lua_tointeger(L, -1)));
            }
            lua_pop(L, 1);
        }

        static void LoadTransform(lua_State* L, int nodeTableIndex, ECS::Prefab& prefab)
        {
            lua_getfield(L, nodeTableIndex, "position");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) prefab.SetPosition(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, nodeTableIndex, "rotation");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Quaternion>::get(L, -1);
                if (result) prefab.SetRotation(result.value());
            }
            lua_pop(L, 1);

            lua_getfield(L, nodeTableIndex, "scale");
            if (lua_isuserdata(L, -1)) {
                auto result = luabridge::Stack<Math::Vector3>::get(L, -1);
                if (result) prefab.SetScale(result.value());
            }
            lua_pop(L, 1);
        }

        static std::unique_ptr<ECS::Prefab> LoadNode(lua_State* L, int nodeTableIndex)
        {
            lua_getfield(L, nodeTableIndex, "name");
            std::string nodeName = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Prefab";
            lua_pop(L, 1);

            auto prefab = std::make_unique<ECS::Prefab>(nodeName);

            lua_getfield(L, nodeTableIndex, "uuid");
            if (lua_isstring(L, -1)) {
                prefab->SetSourceUuid(lua_tostring(L, -1));
            }
            lua_pop(L, 1);

            LoadTransform(L, nodeTableIndex, *prefab);
            LoadCollisionLayer(L, nodeTableIndex, *prefab);
            LoadComponents(L, nodeTableIndex, *prefab);

            // Recursively load children if present
            lua_getfield(L, nodeTableIndex, "children");
            if (lua_istable(L, -1))
            {
                int childCount = static_cast<int>(luaL_len(L, -1));
                int childrenTableIndex = lua_gettop(L);

                for (int i = 1; i <= childCount; i++)
                {
                    lua_geti(L, childrenTableIndex, i);
                    if (lua_istable(L, -1))
                    {
                        int childNodeIndex = lua_gettop(L);
                        std::unique_ptr<ECS::Prefab> childPrefab = LoadNode(L, childNodeIndex);
                        if (childPrefab)
                            prefab->AddChildPrefab(std::move(childPrefab));
                    }
                    lua_pop(L, 1);
                }
            }
            // pop children field (table or nil)
            lua_pop(L, 1);

            return prefab;
        }

        std::unique_ptr<ECS::Prefab> PrefabLoader::Load(const std::string& filePath)
        {
            lua_State* L = luaL_newstate();
            luaL_openlibs(L);
            Scripting::SceneLuaBindings::SetupLuaBindings(L);

            if (luaL_dofile(L, filePath.c_str()) != LUA_OK)
            {
                RTB_ERROR("PrefabLoader: Failed to load file '" + filePath + "': "
                    + std::string(lua_tostring(L, -1)));
                lua_close(L);
                return nullptr;
            }

            if (!lua_istable(L, -1))
            {
                RTB_ERROR("PrefabLoader: File does not return a table: " + filePath);
                lua_close(L);
                return nullptr;
            }

            int rootTableIndex = lua_gettop(L);
            std::unique_ptr<ECS::Prefab> prefab = LoadNode(L, rootTableIndex);

            lua_close(L);
            return prefab;
        }

    }
}
