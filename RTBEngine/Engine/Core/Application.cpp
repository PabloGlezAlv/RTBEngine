#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/LightComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
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


RTBEngine::Core::Application::Application(const ApplicationConfig& cfg)
	: config(cfg), lastTime(0), isRunning(false), physicsSystem(nullptr), physicsAccumulator(0.0f), physicsWorld(nullptr)
{
}


RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

bool RTBEngine::Core::Application::Initialize()
{
	window = std::make_unique<Window>(config.window.title, config.window.width, config.window.height, config.window.fullscreen);
	if (!window->Initialize()) {
		return false;
	}

	lastTime = SDL_GetTicks();

	Scripting::ComponentRegistry::GetInstance().RegisterBuiltInComponents();

	ResourceManager& resources = ResourceManager::GetInstance();
	
	// Shader
	Rendering::Shader* shader = resources.LoadShader(
		"basic",
		"Default/Shaders/basic.vert",
		"Default/Shaders/basic.frag"
	);
	if (!shader) {
		std::cerr << "Failed to load shader" << std::endl;
		return false;
	}

	// Shadow shader
	Rendering::Shader* shadowShader = resources.LoadShader(
		"shadow",
		"Default/Shaders/shadow.vert",
		"Default/Shaders/shadow.frag"
	);
	if (!shadowShader) {
		std::cerr << "Failed to load shadow shader" << std::endl;
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
		// Initialize physics for each BoxColliderComponent
		for (const auto& go : scene->GetGameObjects()) {
			ECS::BoxColliderComponent* boxCollider = go->GetComponent<ECS::BoxColliderComponent>();
			if (boxCollider) {
				physicsSystem->InitializeCollider(go.get(), boxCollider);
			}
		}

		if (scene->GetActiveCamera()) {
			scene->GetActiveCamera()->SetAspectRatio(
				static_cast<float>(config.window.width) / static_cast<float>(config.window.height)
			);
		}

		});

	if (!config.initialScenePath.empty()) {
		if (!sceneMgr.LoadScene(config.initialScenePath)) {
			std::cerr << "Failed to load scene: " << config.initialScenePath << std::endl;
			return false;
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
		ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
		if (scene) {
			while (physicsAccumulator >= config.physics.timeStep) {
                scene->FixedUpdate(config.physics.timeStep);
				physicsSystem->Update(scene, config.physics.timeStep);
				physicsAccumulator -= config.physics.timeStep;
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
		physicsSystem->Update(scene, config.physics.timeStep);
	}

}

void RTBEngine::Core::Application::Render()
{
	ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
	if (!scene) return;

	Rendering::Camera* activeCamera = scene->GetActiveCamera();
	if (!activeCamera) return;

	RenderShadowPass(scene);
	RenderGeometryPass(scene, activeCamera);

	UI::CanvasSystem::GetInstance().Update(scene);
	UI::CanvasSystem::GetInstance().ProcessInput();
	UI::CanvasSystem::GetInstance().RenderAll();

	window->SwapBuffers();
}

void RTBEngine::Core::Application::RenderShadowPass(ECS::Scene* scene)
{
	Rendering::Shader* shadowShader = ResourceManager::GetInstance().GetShader("shadow");
	if (!shadowShader) return;

	shadowShader->Bind();

	for (auto& go : scene->GetGameObjects()) {
		auto* lightComp = go->GetComponent<ECS::LightComponent>();
		if (!lightComp) continue;

		auto* dirLight = dynamic_cast<Rendering::DirectionalLight*>(lightComp->GetLight());
		if (!dirLight || !dirLight->GetCastShadows()) continue;

		Math::Vector3 sceneCenter(0.0f, 2.0f, 0.0f);
		float sceneRadius = 50.0f;
		Math::Matrix4 lightSpaceMatrix = dirLight->GetLightSpaceMatrix(sceneCenter, sceneRadius);

		shadowShader->SetMatrix4("uLightSpaceMatrix", lightSpaceMatrix);

		Rendering::ShadowMap* shadowMap = dirLight->GetShadowMap();
		shadowMap->BindForWriting();

		glViewport(0, 0, shadowMap->GetResolution(), shadowMap->GetResolution());
		glClear(GL_DEPTH_BUFFER_BIT);

		// Disable culling to render all faces (fixes shadow issues with single-sided geometry)
		glDisable(GL_CULL_FACE);
		RenderSceneDepthOnly(scene, shadowShader);
		glEnable(GL_CULL_FACE);

		shadowMap->Unbind();
	}

	glViewport(0, 0, window->GetWidth(), window->GetHeight());
}

void RTBEngine::Core::Application::RenderSceneDepthOnly(ECS::Scene* scene, Rendering::Shader* shader)
{
	for (auto& go : scene->GetGameObjects()) {
		auto* meshRenderer = go->GetComponent<ECS::MeshRenderer>();
		if (!meshRenderer) continue;

		Math::Matrix4 modelMatrix = go->GetTransform().GetModelMatrix();
		shader->SetMatrix4("uModel", modelMatrix);

		auto* animator = go->GetComponent<Animation::Animator>();
		if (animator) {
			shader->SetBool("uHasAnimation", true);
			const auto& boneTransforms = animator->GetBoneTransforms();
			for (size_t i = 0; i < boneTransforms.size() && i < 100; ++i) {
				shader->SetMatrix4("uBoneTransforms[" + std::to_string(i) + "]", boneTransforms[i]);
			}
		}
		else {
			shader->SetBool("uHasAnimation", false);
		}

		for (auto* mesh : meshRenderer->GetMeshes()) {
			mesh->Draw();
		}
	}
}

void RTBEngine::Core::Application::RenderGeometryPass(ECS::Scene* scene, Rendering::Camera* camera)
{
	glClearColor(config.rendering.clearColorR, config.rendering.clearColorG,
		config.rendering.clearColorB, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Rendering::Shader* shader = ResourceManager::GetInstance().GetShader("basic");
	if (!shader) return;

	shader->Bind();

	int pointLightIndex = 0;
	int spotLightIndex = 0;
	Rendering::DirectionalLight* shadowCastingLight = nullptr;

	for (auto& go : scene->GetGameObjects()) {
		auto* lightComp = go->GetComponent<ECS::LightComponent>();
		if (!lightComp || !lightComp->GetLight()) continue;

		Rendering::Light* light = lightComp->GetLight();

		if (light->GetType() == Rendering::LightType::Directional) {
			light->ApplyToShader(shader);

			auto* dirLight = static_cast<Rendering::DirectionalLight*>(light);
			if (dirLight->GetCastShadows()) {
				shadowCastingLight = dirLight;
			}
		}
		else if (light->GetType() == Rendering::LightType::Point) {
			static_cast<Rendering::PointLight*>(light)->ApplyToShader(shader, pointLightIndex++);
		}
		else if (light->GetType() == Rendering::LightType::Spot) {
			static_cast<Rendering::SpotLight*>(light)->ApplyToShader(shader, spotLightIndex++);
		}
	}

	shader->SetInt("numPointLights", pointLightIndex);
	shader->SetInt("numSpotLights", spotLightIndex);

	if (shadowCastingLight) {
		shader->SetBool("uHasShadows", true);
		shader->SetFloat("uShadowBias", shadowCastingLight->GetShadowBias());

		shadowCastingLight->GetShadowMap()->BindForReading(1);
		shader->SetInt("uShadowMap", 1);

		Math::Vector3 sceneCenter(0.0f, 2.0f, 0.0f);
		float sceneRadius = 50.0f;
		Math::Matrix4 lightSpaceMatrix = shadowCastingLight->GetLightSpaceMatrix(sceneCenter, sceneRadius);
		shader->SetMatrix4("uLightSpaceMatrix", lightSpaceMatrix);
	}
	else {
		shader->SetBool("uHasShadows", false);
	}

	scene->Render(camera);
}
