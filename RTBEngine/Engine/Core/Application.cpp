#include "Application.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/LightComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/SphereColliderComponent.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "ResourceManager.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsSystem.h"
#include "../Audio/AudioSystem.h"
#include "../UI/CanvasSystem.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/ScriptManager.h"
#include "../ECS/SceneManager.h"
#include "../Rendering/Skybox.h"
#include "../Rendering/Cubemap.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include "Logger.h"


RTBEngine::Core::Application::Application(const ApplicationConfig& cfg)
	: config(cfg), lastTime(0), isRunning(false), physicsSystem(nullptr), physicsAccumulator(0.0f), physicsWorld(nullptr)
{
}


RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

bool RTBEngine::Core::Application::InitializeImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	if (!ImGui::GetCurrentContext()) {
		RTB_ERROR("Application::InitializeImGui - Failed to create ImGui context");
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplSDL2_InitForOpenGL(window->GetSDLWindow(), SDL_GL_GetCurrentContext())) {
		RTB_ERROR("Application::InitializeImGui - Failed to initialize ImGui SDL2 backend");
		return false;
	}

	if (!ImGui_ImplOpenGL3_Init("#version 330")) {
		RTB_ERROR("Application::InitializeImGui - Failed to initialize ImGui OpenGL3 backend");
		ImGui_ImplSDL2_Shutdown();
		return false;
	}

	return true;
}
 
 void* RTBEngine::Core::Application::GetImGuiContext()
 {
 	return ImGui::GetCurrentContext();
 }

void RTBEngine::Core::Application::ShutdownImGui()
{
	// Ensure the main GL context is current before releasing ImGui GL resources.
	// ViewportsEnable can leave a different context active after rendering.
	if (window) {
		SDL_GL_MakeCurrent(window->GetSDLWindow(), window->GetGLContext());
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

bool RTBEngine::Core::Application::Initialize()
{
	window = std::make_unique<Window>(config.window.title, config.window.width, config.window.height, config.window.fullscreen, config.window.maximized);
	if (!window->Initialize()) {
		RTB_ERROR("Failed to initialize Window");
		return false;
	}

	window->SetResizeCallback([this](int width, int height) {
		OnWindowResized(width, height);
	});

	RTB_INFO("RTBEngine Initializing...");

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
		RTB_ERROR("Failed to load basic shader");
		return false;
	}

	// Shadow shader
	Rendering::Shader* shadowShader = resources.LoadShader(
		"shadow",
		"Default/Shaders/shadow.vert",
		"Default/Shaders/shadow.frag"
	);
	if (!shadowShader) {
		RTB_ERROR("Failed to load shadow shader");
		return false;
	}

	// Skybox shader
	Rendering::Shader* skyboxShader = resources.LoadShader(
		"skybox",
		"Default/Shaders/skybox.vert",
		"Default/Shaders/skybox.frag"
	);
	if (!skyboxShader) {
		RTB_ERROR("Failed to load skybox shader");
		return false;
	}

	// Initialize default skybox
	skybox = resources.GetDefaultSkybox();


	// Initialize physics
	physicsWorld = new Physics::PhysicsWorld();
	physicsWorld->Initialize();
	physicsSystem = new Physics::PhysicsSystem(physicsWorld);

	if (!Audio::AudioSystem::GetInstance().Initialize()) {
		RTB_ERROR("Failed to initialize audio system");
		return false;
	}

	if (!InitializeImGui()) {
		RTB_ERROR("Failed to initialize ImGui");
		return false;
	}

	// Load fonts after ImGui context is ready
	resources.GetDefaultFont();

	RTB_INFO("RTBEngine Initialized Successfully");

	ECS::SceneManager& sceneMgr = ECS::SceneManager::GetInstance();
	sceneMgr.Initialize();

	sceneMgr.SetOnSceneUnloading([this](ECS::Scene* /*scene*/) {
		// Remove all Bullet objects from the world BEFORE GameObjects are destroyed.
		// This prevents btDbvtBroadphase::destroyProxy from accessing a freed proxy
		// when ~BoxColliderComponent() deletes the raw btCollisionObject.
		ResetPhysics();
		});

	sceneMgr.SetOnSceneLoaded([this](ECS::Scene* scene) {
		// Physics was already reset by onSceneUnloading before scene destruction.
		InitializePhysicsForScene(scene);

		if (scene->GetActiveCamera()) {
			scene->GetActiveCamera()->SetAspectRatio(
				static_cast<float>(config.window.width) / static_cast<float>(config.window.height)
			);
		}
		});

	if (!config.initialScenePath.empty()) {
		if (!sceneMgr.LoadScene(config.initialScenePath)) {
			RTB_ERROR("Failed to load scene: " + config.initialScenePath);
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
	if (isShutdown) return;
	isShutdown = true;
	isRunning = false;

	ShutdownImGui();

	if (physicsWorld) {
		physicsWorld->Cleanup();
		delete physicsWorld;
		physicsWorld = nullptr;
	}

	if (physicsSystem) {
		delete physicsSystem;
		physicsSystem = nullptr;
	}

	// Destroy the scene first so script component vtables are still valid during OnDestroy.
	ECS::SceneManager::GetInstance().Shutdown();

	// Only after all GameObjects are destroyed, unload the script DLL.
	Scripting::ScriptManager::GetInstance().UnloadScripts();

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

		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				int newWidth = event.window.data1;
				int newHeight = event.window.data2;
				window->UpdateSize(newWidth, newHeight);
			}
		}

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

	// Begin ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// Render engine UI through the same path as the editor
	Math::Vector2 screenSize(
		static_cast<float>(window->GetWidth()),
		static_cast<float>(window->GetHeight())
	);

	auto& canvasSystem = UI::CanvasSystem::GetInstance();
	canvasSystem.Update(scene);
	canvasSystem.UpdateAllRectTransforms(screenSize);

	// Mouse position in window space (no offset for standalone)
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	canvasSystem.ProcessInput(Math::Vector2(static_cast<float>(mx), static_cast<float>(my)));

	// Render to the background draw list (full screen, no offset)
	canvasSystem.RenderToDrawList(ImGui::GetBackgroundDrawList(), screenSize, Math::Vector2(0.0f, 0.0f));

	// End ImGui frame
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	window->SwapBuffers();
}

void RTBEngine::Core::Application::RenderShadowPass(ECS::Scene* scene)
{
	Rendering::Shader* shadowShader = ResourceManager::GetInstance().GetShader("shadow");
	if (!shadowShader) return;

	shadowShader->Bind();

	for (auto& go : scene->GetGameObjects()) {
		auto* lightComp = go->GetComponent<ECS::LightComponent>();
		if (!lightComp || !lightComp->GetLight()) continue;

		if (lightComp->GetLight()->GetType() != Rendering::LightType::Directional) continue;
		auto* dirLight = static_cast<Rendering::DirectionalLight*>(lightComp->GetLight());
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
		if (!meshRenderer || !meshRenderer->IsEnabled()) continue;

		Math::Matrix4 modelMatrix = go->GetWorldMatrix();
		shader->SetMatrix4("uModel", modelMatrix);

		auto* animator = go->GetComponent<Animation::Animator>();
		if (!animator && go->GetParent()) {
			animator = go->GetParent()->GetComponent<Animation::Animator>();
		}
		if (animator && animator->HasBones()) {
			shader->SetBool("uHasAnimation", true);
			const auto& boneTransforms = animator->GetBoneTransforms();
			for (size_t i = 0; i < boneTransforms.size() && i < 100; ++i) {
				shader->SetMatrix4("uBoneTransforms[" + std::to_string(i) + "]", boneTransforms[i]);
			}
		}
		else {
			shader->SetBool("uHasAnimation", false);
		}

		if (meshRenderer->IsMultiMesh()) {
			for (auto* mesh : meshRenderer->GetMeshes()) {
				if (mesh) mesh->Draw();
			}
		}
		else {
			if (auto* mesh = meshRenderer->GetMesh()) {
				mesh->Draw();
			}
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

	// Render skybox after geometry (uses GL_LEQUAL depth test)
	if (skybox && skybox->IsEnabled() && scene->IsSkyboxEnabled()) {
		// Use scene-specific cubemap if available, otherwise use default
		Rendering::Cubemap* sceneCubemap = scene->GetSkyboxCubemap();
		if (sceneCubemap) {
			skybox->SetCubemap(sceneCubemap);
		}
		else {
			skybox->SetCubemap(ResourceManager::GetInstance().GetDefaultCubemap());
		}
		skybox->Render(camera);
	}

}

void RTBEngine::Core::Application::ResetPhysics()
{
	if (physicsWorld)
		physicsWorld->ResetObjects();

	if (physicsSystem)
		physicsSystem->Reset();

	physicsAccumulator = 0.0f;
}

void RTBEngine::Core::Application::InitializePhysicsForScene(ECS::Scene* scene)
{
	if (!scene || !physicsSystem)
		return;

	for (const auto& go : scene->GetGameObjects())
	{
		ECS::BoxColliderComponent* boxCollider = go->GetComponent<ECS::BoxColliderComponent>();
		if (boxCollider) {
			physicsSystem->InitializeCollider(go.get(), boxCollider);
			continue;
		}

		ECS::SphereColliderComponent* sphereCollider = go->GetComponent<ECS::SphereColliderComponent>();
		if (sphereCollider)
			physicsSystem->InitializeCollider(go.get(), sphereCollider);
	}
}

void RTBEngine::Core::Application::OnWindowResized(int width, int height)
{
	glViewport(0, 0, width, height);

	ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
	if (scene && scene->GetActiveCamera()) {
		float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		scene->GetActiveCamera()->SetAspectRatio(aspectRatio);
	}
}
