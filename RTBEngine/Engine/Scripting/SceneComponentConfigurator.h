#pragma once

struct lua_State;

namespace RTBEngine {
    namespace Scene {
        class GameObject;
        class MeshRenderer;
        class LightComponent;
        class AudioSourceComponent;
        class RigidBodyComponent;
        class BoxColliderComponent;
        class SphereColliderComponent;
        class CapsuleColliderComponent;
        class CameraComponent;
        class FreeLookCamera;
        class NavGridComponent;
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
        class UILayoutGroup;
    }

    namespace Scripting {
        namespace SceneComponentConfigurator {

            void ConfigureCanvas(lua_State* L, int tableIndex, UI::Canvas* comp);
            void ConfigureUIText(lua_State* L, int tableIndex, UI::UIText* comp);
            void ConfigureUIImage(lua_State* L, int tableIndex, UI::UIImage* comp);
            void ConfigureUIPanel(lua_State* L, int tableIndex, UI::UIPanel* comp);
            void ConfigureUIButton(lua_State* L, int tableIndex, UI::UIButton* comp);
            void ConfigureUILayout(lua_State* L, int tableIndex, UI::UILayoutGroup* comp);
            void SyncUIElementProxies(lua_State* L, int tableIndex, UI::UIElement* comp);

            void ConfigureMeshRenderer(lua_State* L, int tableIndex, Scene::MeshRenderer* comp);
            void ConfigureLightComponent(lua_State* L, int tableIndex, Scene::LightComponent* comp);
            void ConfigureAudioSource(lua_State* L, int tableIndex, Scene::AudioSourceComponent* comp);
            void ConfigureRigidBody(lua_State* L, int tableIndex, Scene::RigidBodyComponent* comp, Scene::GameObject* gameObject);
            void ConfigureBoxCollider(lua_State* L, int tableIndex, Scene::BoxColliderComponent* comp, Scene::GameObject* gameObject);
            void ConfigureSphereCollider(lua_State* L, int tableIndex, Scene::SphereColliderComponent* comp, Scene::GameObject* gameObject);
            void ConfigureCapsuleCollider(lua_State* L, int tableIndex, Scene::CapsuleColliderComponent* comp, Scene::GameObject* gameObject);
            void ConfigureCameraComponent(lua_State* L, int tableIndex, Scene::CameraComponent* comp);
            void ConfigureFreeLookCamera(lua_State* L, int tableIndex, Scene::FreeLookCamera* comp);
            void ConfigureAnimator(lua_State* L, int tableIndex, Animation::Animator* comp);
            void ConfigureNavGrid(lua_State* L, int tableIndex, Scene::NavGridComponent* comp);

        }
    }
}

