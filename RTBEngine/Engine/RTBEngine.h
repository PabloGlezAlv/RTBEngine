#pragma once

// Core
#include "Core/ApplicationConfig.h"
#include "Core/Application.h"
#include "Core/ResourceManager.h"
#include "Core/Logger.h"
#include "Core/Event.h"
#include "Core/Time.h"
#include "Core/Version.h"
#include "Core/Scheduler.h"
#include "Core/CountdownTimer.h"
#include "Scene/ComponentQuery.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "ECS/EcsStats.h"
#include "ECS/Components/LocalTransform.h"
#include "ECS/Components/VisualLink.h"
#include "Scripting/LatentActions.h"

// Scene & Components
#include "Scene/GameObject.h"
#include "Scene/Component.h"
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"

// Math
#include "Math/Vectors/Vector2.h"
#include "Math/Vectors/Vector3.h"
#include "Math/Vectors/Vector4.h"
#include "Math/Quaternions/Quaternion.h"
#include "Math/Matrix/Matrix4.h"

// Input
#include "Input/InputManager.h"
#include "Input/KeyCode.h"

// Scripting
#include "Scripting/ComponentRegistry.h"
#include "Data/DataAsset.h"
#include "Data/DataAssetRegistry.h"
#include "Scripting/DataAssetLoader.h"

// Online
#include "Online/IOnlineIdentity.h"
#include "Online/IOnlineLobby.h"
#include "Online/IOnlineTransport.h"
#include "Online/OnlineConfig.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineSystem.h"
#include "Online/OnlineTypes.h"
#include "Online/OnlineUser.h"
#include "Online/OnlineGameplayNet.h"

// Physics layers
#include "Physics/PhysicsLayerSettings.h"

// Navigation
#include "Navigation/NavPathService.h"

// Built-in Components
#include "Scene/MissingComponent.h"
#include "Scene/PrefabInstanceResolver.h"
#include "Scene/PrefabOverrideDiff.h"
#include "Scene/PrefabOverrideOps.h"
#include "Scene/MeshRenderer.h"
#include "Scene/NetworkIdentity.h"
#include "Scene/NetworkTransform.h"
#include "Scene/LightComponent.h"
#include "Scene/VolumeComponent.h"
#include "Scene/AudioSourceComponent.h"
#include "Scene/RigidBodyComponent.h"
#include "Scene/BoxColliderComponent.h"
#include "Scene/SphereColliderComponent.h"
#include "Scene/CapsuleColliderComponent.h"
#include "Scene/NavGridComponent.h"
#include "Scene/NavAgentComponent.h"
#include "Scene/CameraComponent.h"
#include "Scene/TrailRenderer.h"
#include "Scene/ParticleSystem.h"
#include "Scene/AnimatedBillboard.h"

// UI
#include "UI/Canvas.h"
#include "UI/Elements/UIText.h"
#include "UI/Elements/UIImage.h"
#include "UI/Elements/UIPanel.h"
#include "UI/Elements/UIButton.h"
#include "UI/Elements/UISlider.h"
#include "UI/Elements/UIJoystick.h"
