#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/LightComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "ResourceManager.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsSystem.h"
#include "../Audio/AudioSystem.h"
#include "../UI/CanvasSystem.h"
#include "../Scripting/ComponentRegistry.h"
#include "../ECS/SceneManager.h"
#include <backends/imgui_impl_sdl2.h>
#include <iostream>


RTBEngine::Core::Application::Application()
	: lastTime(0), isRunning(false), physicsSystem(nullptr), physicsAccumulator(0.0f), physicsWorld(nullptr)
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

	Scripting::ComponentRegistry::GetInstance().RegisterBuiltInComponents();

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

	ECS::SceneManager& sceneMgr = ECS::SceneManager::GetInstance();
	sceneMgr.Initialize();

	sceneMgr.SetOnSceneLoaded([this](ECS::Scene* scene) {
		// Inicializar physics para cada RigidBody
		for (const auto& go : scene->GetGameObjects()) {
			ECS::RigidBodyComponent* rb = go->GetComponent<ECS::RigidBodyComponent>();
			if (rb) {
				physicsSystem->InitializeRigidBody(go.get(), rb);
			}
		}

		if (scene->GetActiveCamera()) {
			scene->GetActiveCamera()->SetAspectRatio(800.0f / 600.0f);
		}
		});

	if (!sceneMgr.LoadScene("Assets/Scenes/TestScene.lua")) {
		std::cerr << "Failed to load scene" << std::endl;
		return false;
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
		ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
		if (scene) {
			while (physicsAccumulator >= PHYSICS_TIMESTEP) {
				physicsSystem->Update(scene, PHYSICS_TIMESTEP);
				physicsAccumulator -= PHYSICS_TIMESTEP;
			}
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

	ECS::SceneManager::GetInstance().Shutdown();

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
	ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
	if (scene) {
		scene->Update(deltaTime);
		physicsSystem->Update(scene, PHYSICS_TIMESTEP);
	}

}

void RTBEngine::Core::Application::Render()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();

	// Apply lights from scene to shader
	Rendering::Shader* shader = ResourceManager::GetInstance().GetShader("basic");
	if (shader && scene) {
		shader->Bind();

		int pointLightIndex = 0;
		int spotLightIndex = 0;

		for (auto& go : scene->GetGameObjects()) {
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

	Rendering::Camera* activeCamera = scene->GetActiveCamera();
	if (activeCamera) {
		scene->Render(activeCamera);
	}

	UI::CanvasSystem::GetInstance().Update(scene);
	UI::CanvasSystem::GetInstance().ProcessInput();
	UI::CanvasSystem::GetInstance().RenderAll();

	window->SwapBuffers();
}