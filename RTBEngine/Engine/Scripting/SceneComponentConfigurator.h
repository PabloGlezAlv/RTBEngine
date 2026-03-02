#pragma once

struct lua_State;

namespace RTBEngine {
    namespace ECS {
        class GameObject;
        class MeshRenderer;
        class LightComponent;
        class AudioSourceComponent;
        class RigidBodyComponent;
        class BoxColliderComponent;
        class CameraComponent;
        class FreeLookCamera;
    }

    namespace Animation {
        class Animator;
    }

    namespace UI {
        class Canvas;
        class UIText;
        class UIImage;
        class UIPanel;
        class UIButton;
        class UIElement;
    }

    namespace Scripting {
        namespace SceneComponentConfigurator {

            void ConfigureCanvas(lua_State* L, int tableIndex, UI::Canvas* comp);
            void ConfigureUIText(lua_State* L, int tableIndex, UI::UIText* comp);
            void ConfigureUIImage(lua_State* L, int tableIndex, UI::UIImage* comp);
            void ConfigureUIPanel(lua_State* L, int tableIndex, UI::UIPanel* comp);
            void ConfigureUIButton(lua_State* L, int tableIndex, UI::UIButton* comp);
            void SyncUIElementProxies(lua_State* L, int tableIndex, UI::UIElement* comp);

            void ConfigureMeshRenderer(lua_State* L, int tableIndex, ECS::MeshRenderer* comp);
            void ConfigureLightComponent(lua_State* L, int tableIndex, ECS::LightComponent* comp);
            void ConfigureAudioSource(lua_State* L, int tableIndex, ECS::AudioSourceComponent* comp);
            void ConfigureRigidBody(lua_State* L, int tableIndex, ECS::RigidBodyComponent* comp, ECS::GameObject* gameObject);
            void ConfigureBoxCollider(lua_State* L, int tableIndex, ECS::BoxColliderComponent* comp, ECS::GameObject* gameObject);
            void ConfigureCameraComponent(lua_State* L, int tableIndex, ECS::CameraComponent* comp);
            void ConfigureFreeLookCamera(lua_State* L, int tableIndex, ECS::FreeLookCamera* comp);
            void ConfigureAnimator(lua_State* L, int tableIndex, Animation::Animator* comp);

        }
    }
}

