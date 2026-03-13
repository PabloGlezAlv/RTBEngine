#include "PrefabLoader.h"
#include "../ECS/Prefab.h"
#include "../ECS/Component.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/SceneComponentConfigurator.h"
#include "../Scripting/SceneReflectionUtils.h"
#include "../Scripting/SceneLuaBindings.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/LightComponent.h"
#include "../ECS/AudioSourceComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/FreeLookCamera.h"
#include "../Animation/Animator.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UIContainer.h"
#include "../RTBEngine.h"
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>

namespace RTBEngine {
    namespace Scripting {

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

            lua_getfield(L, -1, "name");
            std::string prefabName = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Prefab";
            lua_pop(L, 1);

            auto prefab = std::make_unique<ECS::Prefab>(prefabName);

            lua_getfield(L, -1, "components");
            if (!lua_istable(L, -1))
            {
                RTB_WARN("PrefabLoader: No components table in prefab: " + filePath);
                lua_pop(L, 1);
                lua_close(L);
                return prefab;
            }

            int componentCount = luaL_len(L, -1);
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

                Scripting::SceneReflectionUtils::ApplyLuaTableToComponent(L, compTableIndex, comp);

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

                comp->OnValidate();

                ECS::ComponentSnapshot snap;
                ECS::Prefab::SnapshotComponent(snap, comp);
                prefab->AddSnapshot(std::move(snap));

                delete comp;
                lua_pop(L, 1);
            }

            lua_pop(L, 1);
            lua_close(L);

            RTB_INFO("PrefabLoader: Loaded prefab '" + prefabName + "' from: " + filePath);
            return prefab;
        }

    }
}
