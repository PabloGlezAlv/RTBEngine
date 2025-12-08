#include "Application.h"
#include <GL/glew.h> 
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/GameObject.h"
#include "../ECS/MeshRenderer.h"


#include <iostream>

RTBEngine::Core::Application::Application() 
	: window(nullptr), lastTime(0), isRunning(false), 
	testShader(nullptr), testMesh(nullptr), camera(nullptr), testTexture(nullptr)
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

	// Shader testing
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

	std::vector<Rendering::Vertex> vertices = {
		{ { -0.5f, -0.5f, 0.0f }, { 0,0,1 }, { 0,0 } },
		{ {  0.5f, -0.5f, 0.0f }, { 0,0,1 }, { 1,0 } },
		{ {  0.0f,  0.5f, 0.0f }, { 0,0,1 }, { 0.5f,1 } }
	};

	std::vector<unsigned int> indices = { 0, 1, 2 };

	testMesh = new Rendering::Mesh(vertices, indices);

	testMaterial = new Rendering::Material(testShader);
	testMaterial->SetTexture(testTexture);
	testMaterial->SetColor(Math::Vector4(1.0f, 1.0f, 0.0f, 1.0f));

	camera = new Rendering::Camera(
		Math::Vector3(0.0f, 0.0f, 10.0f),  
		45.0f,                              
		800.0f / 600.0f,                    
		0.1f,                               
		100.0f                              
	);

	camera->SetRotation(0.0f, 180.0f);

	testGameObject = new ECS::GameObject("TestObject");
	ECS::MeshRenderer* meshRenderer = new ECS::MeshRenderer();
	meshRenderer->SetMesh(testMesh);
	meshRenderer->SetMaterial(testMaterial);
	testGameObject->AddComponent(meshRenderer);

	testGameObject->GetTransform().SetPosition(Math::Vector3(0.0f, 0.0f, 0.0f));

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

	if (testGameObject) {
		delete testGameObject;
		testGameObject = nullptr;
	}

	if (testMaterial) {
		delete testMaterial;
		testMaterial = nullptr;
	}

	if (testTexture) {
		delete testTexture;
		testTexture = nullptr;
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
	if (testGameObject) {
		testGameObject->Update(deltaTime);
	}
}

void RTBEngine::Core::Application::Render()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (testGameObject) {
		ECS::MeshRenderer* renderer = testGameObject->GetComponent<ECS::MeshRenderer>();
		if (renderer) {
			renderer->Render(camera);
		}
	}

	window->SwapBuffers();
}