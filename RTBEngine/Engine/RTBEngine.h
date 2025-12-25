#pragma once

// Core
#include "Core/ApplicationConfig.h"
#include "Core/Application.h"
#include "Core/ResourceManager.h"

// ECS
#include "ECS/GameObject.h"
#include "ECS/Component.h"
#include "ECS/Scene.h"
#include "ECS/SceneManager.h"

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

// Built-in Components
#include "ECS/MeshRenderer.h"
#include "ECS/LightComponent.h"
#include "ECS/AudioSourceComponent.h"
#include "ECS/RigidBodyComponent.h"
#include "ECS/BoxColliderComponent.h"
#include "ECS/CameraComponent.h"

// UI
#include "UI/Canvas.h"
#include "UI/Elements/UIText.h"
#include "UI/Elements/UIImage.h"
#include "UI/Elements/UIPanel.h"
#include "UI/Elements/UIButton.h"
