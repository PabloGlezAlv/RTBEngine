#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../ECS/LightComponent.h"

#include <iostream>

RTBEngine::Core::Application::Application()
	: window(nullptr), lastTime(0), isRunning(false),
	testShader(nullptr), testMesh(nullptr), testTexture(nullptr),
	testMaterial(nullptr), testScene(nullptr), camera(nullptr)
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

	testShader = new Rendering::Shader();
	if (!testShader->LoadFromFiles("Assets/Shaders/basic.vert", "Assets/Shaders/basic.frag")) {
		std::cerr << "Failed to load shader!" << std::endl;
		return false;
	}

	testTexture = new Rendering::Texture();
	if (!testTexture->LoadFromFile("Assets/Textures/testTexture.png")) {
		std::cerr << "Failed to load texture!" << std::endl;
		return false;
	}

	// Load 3D model using Assimp
	std::vector<Rendering::Mesh*> loadedMeshes = Rendering::ModelLoader::LoadModel("Assets/Models/cube.obj");

	if (loadedMeshes.empty()) {
		std::cerr << "Failed to load model! Using fallback triangle." << std::endl;

		// Fallback: create a simple triangle
		std::vector<Rendering::Vertex> vertices = {
			{ { -0.5f, -0.5f, 0.0f }, { 0,0,1 }, { 0,0 } },
			{ {  0.5f, -0.5f, 0.0f }, { 0,0,1 }, { 1,0 } },
			{ {  0.0f,  0.5f, 0.0f }, { 0,0,1 }, { 0.5f,1 } }
		};
		std::vector<unsigned int> indices = { 0, 1, 2 };
		testMesh = new Rendering::Mesh(vertices, indices);
	}
	else {
		std::cout << "Successfully loaded model with " << loadedMeshes.size() << " mesh(es)" << std::endl;
		testMesh = loadedMeshes[0]; // Use the first mesh for now
	}

	testMaterial = new Rendering::Material(testShader);
	testMaterial->SetTexture(testTexture);
	testMaterial->SetColor(Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f));  // Cambiado a blanco para ver mejor la luz

	camera = new Rendering::Camera(
		Math::Vector3(0.0f, 2.0f, 5.0f),
		45.0f,
		800.0f / 600.0f,
		0.1f,
		100.0f
	);

	camera->SetRotation(-20.0f, 180.0f);

	testScene = new ECS::Scene("Test Scene");

	ECS::GameObject* modelObj = new ECS::GameObject("LoadedModel");
	ECS::MeshRenderer* meshRenderer = new ECS::MeshRenderer();
	meshRenderer->SetMesh(testMesh);
	meshRenderer->SetMaterial(testMaterial);
	modelObj->AddComponent(meshRenderer);
	modelObj->GetTransform().SetPosition(Math::Vector3(0.0f, 0.0f, 0.0f));

	testScene->AddGameObject(modelObj);

	// Create a directional light (like the sun)
	ECS::GameObject* lightObj = new ECS::GameObject("MainLight");
	Rendering::DirectionalLight* dirLight = new Rendering::DirectionalLight(
		Math::Vector3(0.0f, -1.0f, -0.3f),  // Direction (from above, slightly angled)
		Math::Vector3(1.0f, 1.0f, 1.0f)     // White light
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

	if (testMaterial) {
		delete testMaterial;
		testMaterial = nullptr;
	}

	if (testTexture) {
		delete testTexture;
		testTexture = nullptr;
	}

	if (testMesh) {
		delete testMesh;
		testMesh = nullptr;
	}

	if (camera) {
		delete camera;
		camera = nullptr;
	}

	if (testShader) {
		delete testShader;
		testShader = nullptr;
	}

	if (window)
	{
		delete window;
		window = nullptr;
	}
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