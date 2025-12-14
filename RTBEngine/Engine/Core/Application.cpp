#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../ECS/LightComponent.h"
#include "ResourceManager.h"

#include <iostream>

RTBEngine::Core::Application::Application()
	: window(nullptr), lastTime(0), isRunning(false),
	 testScene(nullptr), camera(nullptr)
{
}

RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

bool RTBEngine::Core::Application::Initialize()
{
	window = new Window("RTBEngine - Application", 800, 600);
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
	camera = new Rendering::Camera(
		Math::Vector3(0.0f, 2.0f, 5.0f),
		45.0f,
		800.0f / 600.0f,
		0.1f,
		100.0f
	);
	camera->SetRotation(-20.0f, 180.0f);

	// Create scene
	testScene = new ECS::Scene("Test Scene");

	// Create model GameObject
	ECS::GameObject* modelObj = new ECS::GameObject("LoadedModel");
	ECS::MeshRenderer* meshRenderer = new ECS::MeshRenderer();
	meshRenderer->SetMesh(testMesh);
	meshRenderer->SetMaterial(material);
	modelObj->AddComponent(meshRenderer);
	modelObj->GetTransform().SetPosition(Math::Vector3(0.0f, 0.0f, 0.0f));
	testScene->AddGameObject(modelObj);

	// Create directional light
	ECS::GameObject* lightObj = new ECS::GameObject("MainLight");
	Rendering::DirectionalLight* dirLight = new Rendering::DirectionalLight(
		Math::Vector3(0.0f, -1.0f, -0.3f),
		Math::Vector3(1.0f, 1.0f, 1.0f)
	);
	dirLight->SetIntensity(1.0f);

	ECS::LightComponent* lightComponent = new ECS::LightComponent(dirLight);
	lightObj->AddComponent(lightComponent);
	testScene->AddGameObject(lightObj);

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

		Render();
	}
}

void RTBEngine::Core::Application::Shutdown()
{
	isRunning = false;

	if (testScene) {
		delete testScene;
		testScene = nullptr;
	}

	if (camera) {
		delete camera;
		camera = nullptr;
	}

	if (window) {
		delete window;
		window = nullptr;
	}

	// Clear all cached resources
	ResourceManager::GetInstance().Clear();
}

void RTBEngine::Core::Application::ProcessInput()
{
	Input::InputManager& input = Input::InputManager::GetInstance();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
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

	if (testScene) {
		testScene->Render(camera);
	}

	window->SwapBuffers();
}