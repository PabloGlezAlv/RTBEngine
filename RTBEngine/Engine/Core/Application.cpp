#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../ECS/RigidBodyComponent.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../ECS/LightComponent.h"
#include "ResourceManager.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsSystem.h"
#include "../Physics/RigidBody.h"
#include "../Physics/BoxCollider.h"
#include "../Audio/AudioSystem.h"
#include "../Audio/AudioClip.h"
#include "../ECS/AudioSourceComponent.h"
#include "../UI/CanvasSystem.h"
#include "../UI/Canvas.h"
#include "../UI/Elements/UIPanel.h"
#include "../UI/Elements/UIText.h"
#include "../UI/Elements/UIImage.h"
#include "../UI/Elements/UIButton.h"
#include "../UI/Elements/UIButton.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/SceneLoader.h"

#include <backends/imgui_impl_sdl2.h>
#include <iostream>

RTBEngine::Core::Application::Application()
	: lastTime(0), isRunning(false), testMesh(nullptr), physicsSystem(nullptr), physicsAccumulator(0.0f), physicsWorld(nullptr)
{
}

RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

bool RTBEngine::Core::Application::Initialize()
{
	window = std::make_unique<Window>("RTBEngine - Application", 800, 600);
	if (!window->Initialize()) {
		return false;
	}

	lastTime = SDL_GetTicks();

	RegisterBuiltInComponents();

	ResourceManager& resources = ResourceManager::GetInstance();
	
	// Shader
	Rendering::Shader* shader = resources.LoadShader(
		"basic",
		"Assets/Shaders/basic.vert",
		"Assets/Shaders/basic.frag"
	);
	if (!shader) {
		std::cerr << "Failed to load shader" << std::endl;
		return false;
	}

	// Texture
	Rendering::Texture* texture = resources.LoadTexture("Assets/Textures/testTexture.png");
	if (!texture) {
		std::cerr << "Failed to load texture" << std::endl;
		return false;
	}

	// Load 3D model
	testMesh = resources.LoadModel("Assets/Models/cube.obj");
	if (!testMesh) {
		std::cerr << "Failed to load model" << std::endl;
		return false;
	}

	// Create material using loaded shader and texture
	Rendering::Material* material = new Rendering::Material(shader);
	material->SetTexture(texture);
	material->SetColor(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	// Create camera
	camera = std::make_unique<Rendering::Camera>(
		Math::Vector3(0.0f, 0.0f, 10.0f),
		45.0f,
		800.0f / 600.0f,
		0.1f,
		100.0f
	);
	camera->SetRotation(-15.0f, 180.0f);

	// Initialize physics
	physicsWorld = new Physics::PhysicsWorld();
	physicsWorld->Initialize();
	physicsSystem = new Physics::PhysicsSystem(physicsWorld);

	if (!Audio::AudioSystem::GetInstance().Initialize()) {
		std::cerr << "Failed to initialize audio system" << std::endl;
		return false;
	}

	if (!UI::CanvasSystem::GetInstance().Initialize(window->GetSDLWindow())) {
		std::cerr << "Failed to initialize CanvasSystem" << std::endl;
		return false;
	}

	// Create scene from Lua
	testScene.reset(Scripting::SceneLoader::LoadScene("Assets/Scenes/TestScene.lua"));
    if (!testScene) {
        std::cerr << "Failed to load scene from Lua" << std::endl;
        return false;
    }

	// Register loaded objects with PhysicsSystem
	for (const auto& go : testScene->GetGameObjects()) {
		ECS::RigidBodyComponent* rbComp = go->GetComponent<ECS::RigidBodyComponent>();
		if (rbComp) {
			physicsSystem->InitializeRigidBody(go.get(), rbComp);
		}
	}

	return true;
}

void RTBEngine::Core::Application::Run()
{
	isRunning = true;

	while (isRunning)
	{
		Input::InputManager::GetInstance().Update();

		ProcessInput();

		Uint32 currentTime = SDL_GetTicks();
		float deltaTime = (currentTime - lastTime) / 1000.0f;
		lastTime = currentTime;

		Update(deltaTime);

		Audio::AudioSystem::GetInstance().Update();

		// Fixed timestep physics update
		physicsAccumulator += deltaTime;
		while (physicsAccumulator >= PHYSICS_TIMESTEP) {
			physicsSystem->Update(testScene.get(), PHYSICS_TIMESTEP);
			physicsAccumulator -= PHYSICS_TIMESTEP;
		}

		Render();
	}
}

void RTBEngine::Core::Application::Shutdown()
{
	isRunning = false;

	UI::CanvasSystem::GetInstance().Shutdown();

	if (physicsWorld) {
		physicsWorld->Cleanup();
		delete physicsWorld;
		physicsWorld = nullptr;
	}

	if (physicsSystem) {
		delete physicsSystem;
		physicsSystem = nullptr;
	}

	testScene.reset();
	camera.reset();

	ResourceManager::GetInstance().Clear();

	Audio::AudioSystem::GetInstance().Shutdown();

	window.reset();
}

void RTBEngine::Core::Application::ProcessInput()
{
	Input::InputManager& input = Input::InputManager::GetInstance();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		input.ProcessEvent(event);

		if (event.type == SDL_QUIT)
			isRunning = false;
	}

	if (input.IsKeyJustPressed(Input::KeyCode::Escape))
		isRunning = false;
}

void RTBEngine::Core::Application::Update(float deltaTime)
{
	if (testScene) {
		testScene->Update(deltaTime);
	}
}

void RTBEngine::Core::Application::Render()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Apply lights from scene to shader
	Rendering::Shader* shader = ResourceManager::GetInstance().GetShader("basic");
	if (shader && testScene) {
		shader->Bind();

		int pointLightIndex = 0;
		int spotLightIndex = 0;

		for (auto& go : testScene->GetGameObjects()) {
			ECS::LightComponent* lightComp = go->GetComponent<ECS::LightComponent>();
			if (lightComp && lightComp->GetLight()) {
				Rendering::Light* light = lightComp->GetLight();

				if (light->GetType() == Rendering::LightType::Directional) {
					light->ApplyToShader(shader);
				}
				else if (light->GetType() == Rendering::LightType::Point) {
					auto* pointLight = static_cast<Rendering::PointLight*>(light);
					pointLight->ApplyToShader(shader, pointLightIndex++);
				}
				else if (light->GetType() == Rendering::LightType::Spot) {
					auto* spotLight = static_cast<Rendering::SpotLight*>(light);
					spotLight->ApplyToShader(shader, spotLightIndex++);
				}
			}
		}

		shader->SetInt("numPointLights", pointLightIndex);
		shader->SetInt("numSpotLights", spotLightIndex);
	}

	if (testScene) {
		testScene->Render(camera.get());
	}

	UI::CanvasSystem::GetInstance().Update(testScene.get());
	UI::CanvasSystem::GetInstance().ProcessInput();
	UI::CanvasSystem::GetInstance().RenderAll();

	window->SwapBuffers();
}

void RTBEngine::Core::Application::RegisterBuiltInComponents() {
	Scripting::ComponentRegistry& registry = Scripting::ComponentRegistry::GetInstance();

	// ECS Components
	registry.RegisterComponent("MeshRenderer", []() {
		return new ECS::MeshRenderer();
		});

	// Physics Components
	registry.RegisterComponent("RigidBodyComponent", []() {
		return new ECS::RigidBodyComponent();
		});

	// Rendering Components
	registry.RegisterComponent("LightComponent", []() {
		return new ECS::LightComponent();
		});

	// Audio Components
	registry.RegisterComponent("AudioSourceComponent", []() {
		return new ECS::AudioSourceComponent();
		});

	// UI Components
	registry.RegisterComponent("Canvas", []() {
		return new UI::Canvas();
		});

	registry.RegisterComponent("UIImage", []() {
		return new UI::UIImage();
		});

	registry.RegisterComponent("UIPanel", []() {
		return new UI::UIPanel();
		});

	registry.RegisterComponent("UIText", []() {
		return new UI::UIText();
		});

	registry.RegisterComponent("UIButton", []() {
		return new UI::UIButton();
		});
}