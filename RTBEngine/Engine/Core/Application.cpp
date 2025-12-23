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

	// Create scene
	testScene = std::make_unique<ECS::Scene>("Test Scene");

	// Create directional light
	ECS::GameObject* lightObj = new ECS::GameObject("MainLight");
	auto dirLight = std::make_unique<Rendering::DirectionalLight>(
		Math::Vector3(0.0f, -1.0f, -0.3f),
		Math::Vector3(1.0f, 1.0f, 1.0f)
	);
	dirLight->SetIntensity(1.0f);

	ECS::LightComponent* lightComponent = new ECS::LightComponent(std::move(dirLight));
	lightObj->AddComponent(lightComponent);
	testScene->AddGameObject(lightObj);

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

	CreatePhysicsTestScene();

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
	Audio::AudioSystem::GetInstance().Shutdown();

	// Cleanup Physics
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
	window.reset();

	ResourceManager::GetInstance().Clear();
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
	UI::CanvasSystem::GetInstance().RenderAll();

	window->SwapBuffers();
}


void RTBEngine::Core::Application::CreatePhysicsTestScene()
{
	ResourceManager& resources = ResourceManager::GetInstance();
	Rendering::Shader* shader = resources.GetShader("basic");
	Rendering::Texture* texture = resources.GetTexture("Assets/Textures/testTexture.png");

	Rendering::Material* material = new Rendering::Material(shader);
	material->SetTexture(texture);
	material->SetColor(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	ECS::GameObject* ground = new ECS::GameObject("Ground");

	ECS::MeshRenderer* groundMeshRenderer = new ECS::MeshRenderer();
	groundMeshRenderer->SetMesh(testMesh);
	groundMeshRenderer->SetMaterial(material);
	ground->AddComponent(groundMeshRenderer);

	ground->GetTransform().SetPosition(Math::Vector3(0.0f, -5.0f, 0.0f));
	ground->GetTransform().SetScale(Math::Vector3(10.0f, 1.0f, 10.0f));

	auto groundRigidBody = std::make_unique<Physics::RigidBody>();
	groundRigidBody->SetType(Physics::RigidBodyType::Static);
	groundRigidBody->SetMass(0.0f);
	groundRigidBody->SetFriction(0.7f);

	auto groundCollider = std::make_unique<Physics::BoxCollider>(testMesh);
	groundCollider->SetSize(groundCollider->GetSize() * ground->GetTransform().GetScale());

	// Create RigidBodyComponent
	ECS::RigidBodyComponent* groundRBComponent = new ECS::RigidBodyComponent();
	groundRBComponent->SetRigidBody(std::move(groundRigidBody));
	groundRBComponent->SetCollider(std::move(groundCollider));
	ground->AddComponent(groundRBComponent);

	testScene->AddGameObject(ground);

	physicsSystem->InitializeRigidBody(ground, groundRBComponent);

	ECS::GameObject* cube = new ECS::GameObject("FallingCube");

	ECS::MeshRenderer* cubeMeshRenderer = new ECS::MeshRenderer();
	cubeMeshRenderer->SetMesh(testMesh);
	cubeMeshRenderer->SetMaterial(material);
	cube->AddComponent(cubeMeshRenderer);

	cube->GetTransform().SetPosition(Math::Vector3(0.0f, 3.0f, 0.0f));
	cube->GetTransform().SetScale(Math::Vector3(1.0f, 1.0f, 1.0f));

	auto cubeRigidBody = std::make_unique<Physics::RigidBody>();
	cubeRigidBody->SetType(Physics::RigidBodyType::Dynamic);
	cubeRigidBody->SetMass(1.0f);
	cubeRigidBody->SetFriction(0.5f);
	cubeRigidBody->SetRestitution(1.0f);

	auto cubeCollider = std::make_unique<Physics::BoxCollider>(testMesh);

	ECS::RigidBodyComponent* cubeRBComponent = new ECS::RigidBodyComponent();
	cubeRBComponent->SetRigidBody(std::move(cubeRigidBody));
	cubeRBComponent->SetCollider(std::move(cubeCollider));
	cube->AddComponent(cubeRBComponent);

	testScene->AddGameObject(cube);

	physicsSystem->InitializeRigidBody(cube, cubeRBComponent);

	Audio::AudioClip* testClip = ResourceManager::GetInstance().LoadAudioClip("Assets/Audio/test.mp3");
	auto* audioSource = new ECS::AudioSourceComponent();
	audioSource->SetClip(testClip);
	audioSource->SetVolume(0.5f);
	audioSource->SetLoop(true);
	audioSource->SetPlayOnStart(true);
	cube->AddComponent(audioSource);

	//PointLight
	ECS::GameObject* pointLightObj = new ECS::GameObject("PointLight");
	pointLightObj->GetTransform().SetPosition(Math::Vector3(3.0f, 4.0f, 0.0f));

	auto pointLight = std::make_unique<Rendering::PointLight>();
	pointLight->SetColor(Math::Vector3(0.2f, 0.5f, 1.0f)); 
	pointLight->SetIntensity(15.0f);
	pointLight->SetRange(30.0f);

	ECS::LightComponent* pointLightComp = new ECS::LightComponent(std::move(pointLight));
	pointLightObj->AddComponent(pointLightComp);
	testScene->AddGameObject(pointLightObj);

	// SpotLight
	ECS::GameObject* spotLightObj = new ECS::GameObject("SpotLight");
	spotLightObj->GetTransform().SetPosition(Math::Vector3(-3.0f, 6.0f, 0.0f));
	spotLightObj->GetTransform().SetRotation(Math::Quaternion::FromEulerAngles(90.0f, 0.0f, 0.0f));  // Apunta hacia abajo

	auto spotLight = std::make_unique<Rendering::SpotLight>();
	spotLight->SetColor(Math::Vector3(1.0f, 0.0f, 0.0f));
	spotLight->SetIntensity(50.0f);
	spotLight->SetRange(50.0f);
	spotLight->SetCutOff(5.0f, 15.0f);

	ECS::LightComponent* spotLightComp = new ECS::LightComponent(std::move(spotLight));
	spotLightObj->AddComponent(spotLightComp);
	testScene->AddGameObject(spotLightObj);

	ECS::GameObject* canvasObj = new ECS::GameObject("UICanvas");
	UI::Canvas* canvas = new UI::Canvas();
	canvasObj->AddComponent(canvas);
	canvas->SetSortOrder(0);
	testScene->AddGameObject(canvasObj);

	ECS::GameObject* panelObj = new ECS::GameObject("TestPanel");
	UI::UIPanel* panel = new UI::UIPanel();
	panelObj->AddComponent(panel);
	testScene->AddGameObject(panelObj);
	panelObj->SetParent(canvasObj);
	panel->GetRectTransform()->SetAnchoredPosition(Math::Vector2(0.0f, 0.0f));
	panel->GetRectTransform()->SetSize(Math::Vector2(400.0f, 300.0f));
	panel->SetBackgroundColor(Math::Vector4(0.2f, 0.4f, 0.8f, 0.8f));
	panel->SetBorderColor(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	panel->SetBorderThickness(2.0f);
	panel->SetHasBorder(true);
}