#include "Application.h"
#include "Window.h"
#include "Time.h"
#include "../Input/Input.h"
#include "../Rendering/Rendering.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../ECS/LightComponent.h"
#include "../ECS/RigidBodyComponent.h"
#include "../ECS/CameraComponent.h"
#include "../ECS/BoxColliderComponent.h"
#include "../ECS/SphereColliderComponent.h"
#include "../ECS/CapsuleColliderComponent.h"
#include "../ECS/MeshRenderer.h"
#include "../Animation/Animator.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "ResourceManager.h"
#include "../Physics/PhysicsWorld.h"
#include "../Physics/PhysicsLayerSettings.h"

#include <filesystem>
#include "../Physics/PhysicsSystem.h"
#include "../Audio/AudioSystem.h"
#include "../UI/CanvasSystem.h"
#include "../Scripting/ComponentRegistry.h"
#include "../Scripting/ScriptManager.h"
#include "../ECS/PrefabRegistry.h"
#include "../ECS/SceneManager.h"
#include "../Rendering/Skybox.h"
#include "../Rendering/Cubemap.h"
#include "../Rendering/Frustum.h"
#include "../Online/OnlineSystem.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include <atomic>
#include "Logger.h"

namespace {
	std::atomic_bool g_quitRequested{ false };

	bool IsMouseUiEvent(const SDL_Event& event)
	{
		switch (event.type) {
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEWHEEL:
			return true;
		default:
			return false;
		}
	}

	bool IsMouseOwnedByGameplay(const RTBEngine::Core::Window* window)
	{
		if (!window) {
			return false;
		}

		return window->IsMouseCaptured() || !window->IsCursorVisible();
	}

	void CollectHierarchy(RTBEngine::ECS::GameObject* root,
		std::vector<RTBEngine::ECS::GameObject*>& outHierarchy)
	{
		if (!root) {
			return;
		}

		outHierarchy.push_back(root);
		for (RTBEngine::ECS::GameObject* child : root->GetChildren()) {
			CollectHierarchy(child, outHierarchy);
		}
	}

	RTBEngine::Animation::Animator* FindAnimatorInAncestors(RTBEngine::ECS::GameObject* start)
	{
		for (RTBEngine::ECS::GameObject* current = start; current; current = current->GetParent()) {
			if (auto* animator = current->GetComponent<RTBEngine::Animation::Animator>()) {
				return animator;
			}
		}
		return nullptr;
	}
}

RTBEngine::Core::Application::Application(const ApplicationConfig& cfg)
	: config(cfg), lastTime(0), isRunning(false), physicsSystem(nullptr), physicsWorld(nullptr)
{
}


RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

void RTBEngine::Core::Application::RequestQuit()
{
	g_quitRequested.store(true, std::memory_order_release);
}

bool RTBEngine::Core::Application::IsQuitRequested()
{
	return g_quitRequested.load(std::memory_order_acquire);
}

bool RTBEngine::Core::Application::ConsumeQuitRequest()
{
	return g_quitRequested.exchange(false, std::memory_order_acq_rel);
}

void RTBEngine::Core::Application::ClearQuitRequest()
{
	g_quitRequested.store(false, std::memory_order_release);
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
	ClearQuitRequest();

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
	Time::Reset();
	Time::SetFixedDeltaTime(config.physics.timeStep);

	if (!Online::OnlineSystem::GetInstance().Initialize(config.online)) {
		RTB_ERROR("Failed to initialize OnlineSystem");
		return false;
	}

	Scripting::ComponentRegistry::GetInstance().RegisterBuiltInComponents();

	// Load user scripts DLL if present next to the executable
	if (!Scripting::ScriptManager::GetInstance().IsLoaded()) {
		namespace fs = std::filesystem;
		fs::path scriptsDll = fs::current_path() / "GameScripts.dll";
		if (fs::exists(scriptsDll)) {
			Scripting::ScriptManager::GetInstance().LoadScripts(scriptsDll.string());
		}
	}

	// Prefabs can contain script components, so load them after the script DLL
	// and before any scene tries to instantiate prefab references.
	if (!config.initialScenePath.empty()) {
		namespace fs = std::filesystem;
		const fs::path assetsPath = fs::current_path() / "Assets";
		ECS::PrefabRegistry::GetInstance().Clear();
		if (fs::exists(assetsPath) && fs::is_directory(assetsPath)) {
			ECS::PrefabRegistry::GetInstance().LoadAll(assetsPath.string());
		}
	}

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

	Rendering::Shader* uiWorldShader = resources.LoadShader(
		"ui_world",
		"Default/Shaders/ui_world.vert",
		"Default/Shaders/ui_world.frag"
	);
	if (!uiWorldShader) {
		RTB_ERROR("Failed to load world-space UI shader");
		return false;
	}

	// Initialize default skybox
	skybox = resources.GetDefaultSkybox();


	// Collision layers (project physics_layers.ini in working directory)
	{
		auto& layerSettings = Physics::PhysicsLayerSettings::Get();
		const std::filesystem::path layerSettingsPath =
			std::filesystem::current_path() / Physics::PhysicsLayerSettings::GetDefaultSettingsFileName();
		if (!layerSettings.LoadFromFile(layerSettingsPath)) {
			layerSettings.ResetToDefaults();
		}
	}

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
	sceneMgr.SetOnHierarchyAdded([this](ECS::GameObject* root) {
		InitializePhysicsForHierarchy(root);
	});
	sceneMgr.SetOnHierarchyDeactivated([this](ECS::GameObject* root) {
		DetachPhysicsHierarchy(root);
	});

	sceneMgr.SetOnSceneUnloading([this](ECS::Scene* /*scene*/) {
		UI::CanvasSystem::GetInstance().ClearState();
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
		if (ConsumeQuitRequest()) {
			isRunning = false;
			break;
		}

		Input::InputManager::GetInstance().Update();

		ProcessInput();

		if (ConsumeQuitRequest()) {
			isRunning = false;
			break;
		}

		Uint32 currentTime = SDL_GetTicks();
		float deltaTime = (currentTime - lastTime) / 1000.0f;
		lastTime = currentTime;

		Update(deltaTime);

		if (ConsumeQuitRequest()) {
			isRunning = false;
			break;
		}

		Audio::AudioSystem::GetInstance().Update();


		Render();

		if (ConsumeQuitRequest()) {
			isRunning = false;
			break;
		}
	}
}

void RTBEngine::Core::Application::Shutdown()
{
	if (isShutdown) return;
	isShutdown = true;
	isRunning = false;
	ClearQuitRequest();

	Online::OnlineSystem::GetInstance().Shutdown();

	ShutdownImGui();

	// Destroy the scene while physics is still alive so component OnDestroy paths
	// can detach Bullet state through the normal scene-unloading callback.
	ECS::SceneManager::GetInstance().Shutdown();

	if (physicsSystem) {
		delete physicsSystem;
		physicsSystem = nullptr;
	}

	if (physicsWorld) {
		physicsWorld->Cleanup();
		delete physicsWorld;
		physicsWorld = nullptr;
	}

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
		if (!(IsMouseOwnedByGameplay(window.get()) && IsMouseUiEvent(event))) {
			ImGui_ImplSDL2_ProcessEvent(&event);
		}
		input.ProcessEvent(event);

		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				int newWidth = event.window.data1;
				int newHeight = event.window.data2;
				window->UpdateSize(newWidth, newHeight);
			}
		}

		if (event.type == SDL_QUIT) {
			if (onQuitRequested)
				onQuitRequested();
			else
				isRunning = false;
		}
	}

}

void RTBEngine::Core::Application::Update(float deltaTime)
{
	if (IsQuitRequested()) {
		return;
	}

	Time::SetFixedDeltaTime(config.physics.timeStep);
	Time::AdvanceFrame(deltaTime);

	Online::OnlineSystem::GetInstance().Tick(deltaTime);

	ECS::SceneManager& sceneMgr = ECS::SceneManager::GetInstance();
	sceneMgr.ProcessPendingSceneLoad();

	if (IsQuitRequested()) {
		return;
	}

	ECS::Scene* scene = sceneMgr.GetActiveScene();
	if (!scene) return;

	if (physicsSystem) {
		physicsSystem->SyncRenderTransforms(scene, 1.0f);
	}

	scene->Update(deltaTime);

	if (IsQuitRequested()) {
		return;
	}

	if (sceneMgr.ProcessPendingSceneLoad()) {
		return;
	}

	if (IsQuitRequested()) {
		return;
	}

	scene = sceneMgr.GetActiveScene();
	if (!scene) return;

	// Fixed timestep physics update
	while (Time::ConsumeFixedStep()) {
		scene->FixedUpdate(Time::GetFixedDeltaTime());
		if (physicsSystem) {
			physicsSystem->Update(scene, Time::GetFixedDeltaTime());
		}
	}

	if (physicsSystem) {
		physicsSystem->SyncRenderTransforms(scene, Time::GetFixedInterpolationAlpha());
	}

	scene->LateUpdate(deltaTime);
}

void RTBEngine::Core::Application::Render()
{
	ECS::Scene* scene = ECS::SceneManager::GetInstance().GetActiveScene();
	if (!scene) return;

	Rendering::Camera* activeCamera = scene->GetActiveCamera();
	if (!activeCamera) return;

	if (ECS::CameraComponent* activeCameraComponent = scene->GetMainCamera()) {
		activeCameraComponent->SyncNow();
		activeCamera = activeCameraComponent->GetCamera();
		if (!activeCamera) {
			return;
		}
	}

	auto& canvasSystem = UI::CanvasSystem::GetInstance();
	canvasSystem.Update(scene);

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

	canvasSystem.UpdateAllRectTransforms(screenSize);

	// Mouse position in window space (no offset for standalone)
	if (!IsMouseOwnedByGameplay(window.get())) {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		canvasSystem.ProcessInput(Math::Vector2(static_cast<float>(mx), static_cast<float>(my)));
	}

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
		if (!go || !go->IsActiveInHierarchy()) continue;

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
		// Frustum for lighning
		Rendering::Frustum shadowFrustum;
		shadowFrustum.ExtractPlanes(lightSpaceMatrix);
		RenderSceneDepthOnly(scene, shadowShader, shadowFrustum);

		glEnable(GL_CULL_FACE);

		shadowMap->Unbind();
	}

	glViewport(0, 0, window->GetWidth(), window->GetHeight());
}

void RTBEngine::Core::Application::RenderSceneDepthOnly(ECS::Scene* scene, Rendering::Shader* shader, const Rendering::Frustum& frustum)
{
	for (auto& go : scene->GetGameObjects()) {
		if (!go || !go->IsActiveInHierarchy()) continue;

		auto* meshRenderer = go->GetComponent<ECS::MeshRenderer>();
		if (!meshRenderer || !meshRenderer->IsEnabled()) continue;

		// Frustum culling
		Math::Vector3 localMin, localMax;
		meshRenderer->GetCombinedAABB(localMin, localMax);
		if (localMin != localMax) {
			Math::Vector3 worldMin, worldMax;
			Rendering::Frustum::TransformAABB(go->GetWorldMatrix(), localMin, localMax, worldMin, worldMax);
			if (!frustum.IsAABBVisible(worldMin, worldMax)) continue;
		}

		Math::Matrix4 modelMatrix = go->GetWorldMatrix();
		shader->SetMatrix4("uModel", modelMatrix);

		Animation::Animator* animator = FindAnimatorInAncestors(go.get());
		if (animator && animator->ShouldSkinMesh()) {
			shader->SetBool("uHasAnimation", true);
			shader->SetBoneTransforms(animator->GetBoneTransforms());
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
		if (!go || !go->IsActiveInHierarchy()) continue;

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

	// Render skybox after opaque geometry (uses GL_LEQUAL depth test)
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

	// Transparent effects must render after the skybox because they skip depth writes.
	scene->RenderTransparentEffects(camera);

	UI::CanvasSystem::GetInstance().RenderWorldSpace(camera);
}

void RTBEngine::Core::Application::ResetPhysics()
{
	if (physicsWorld)
		physicsWorld->ResetObjects();

	if (physicsSystem)
		physicsSystem->Reset();

}

void RTBEngine::Core::Application::RebuildPhysicsForScene(ECS::Scene* scene)
{
	if (!scene) {
		ResetPhysics();
		return;
	}

	for (const auto& gameObject : scene->GetGameObjects()) {
		if (!gameObject) {
			continue;
		}

		DetachPhysicsFromGameObject(gameObject.get());
	}

	ResetPhysics();
	InitializePhysicsForScene(scene);
}

void RTBEngine::Core::Application::InitializePhysicsForGameObject(ECS::GameObject* gameObject)
{
	if (!gameObject || !physicsSystem) {
		return;
	}

	ECS::BoxColliderComponent* boxCollider = gameObject->GetComponent<ECS::BoxColliderComponent>();
	if (boxCollider) {
		if (!boxCollider->GetBulletCollisionObject() && !boxCollider->GetPhysicsWorld()) {
			physicsSystem->InitializeCollider(gameObject, boxCollider);
		}
		return;
	}

	ECS::SphereColliderComponent* sphereCollider = gameObject->GetComponent<ECS::SphereColliderComponent>();
	if (sphereCollider) {
		if (!sphereCollider->GetBulletCollisionObject() && !sphereCollider->GetPhysicsWorld()) {
			physicsSystem->InitializeCollider(gameObject, sphereCollider);
		}
		return;
	}

	ECS::CapsuleColliderComponent* capsuleCollider = gameObject->GetComponent<ECS::CapsuleColliderComponent>();
	if (capsuleCollider &&
		!capsuleCollider->GetBulletCollisionObject() &&
		!capsuleCollider->GetPhysicsWorld()) {
		physicsSystem->InitializeCollider(gameObject, capsuleCollider);
	}
}

void RTBEngine::Core::Application::InitializePhysicsForScene(ECS::Scene* scene)
{
	if (!scene || !physicsSystem)
		return;

	for (const auto& go : scene->GetGameObjects())
	{
		InitializePhysicsForGameObject(go.get());
	}
}

void RTBEngine::Core::Application::InitializePhysicsForHierarchy(ECS::GameObject* root)
{
	if (!root) {
		return;
	}

	std::vector<ECS::GameObject*> hierarchy;
	CollectHierarchy(root, hierarchy);

	for (ECS::GameObject* gameObject : hierarchy) {
		InitializePhysicsForGameObject(gameObject);
	}
}

void RTBEngine::Core::Application::DetachPhysicsFromGameObject(ECS::GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	ECS::BoxColliderComponent* boxCollider = gameObject->GetComponent<ECS::BoxColliderComponent>();
	ECS::SphereColliderComponent* sphereCollider = gameObject->GetComponent<ECS::SphereColliderComponent>();
	ECS::CapsuleColliderComponent* capsuleCollider = gameObject->GetComponent<ECS::CapsuleColliderComponent>();
	ECS::RigidBodyComponent* rbComp = gameObject->GetComponent<ECS::RigidBodyComponent>();
	Physics::RigidBody* rigidBody = (rbComp && rbComp->HasRigidBody()) ? rbComp->GetRigidBody() : nullptr;
	btRigidBody* bulletBody = rigidBody ? rigidBody->GetBulletRigidBody() : nullptr;

	auto clearDynamicColliderRef = [bulletBody](auto* collider) {
		if (!collider || collider->GetBulletCollisionObject() != bulletBody) {
			return;
		}

		collider->SetPhysicsWorld(nullptr);
		collider->SetBulletCollisionObject(nullptr, false);
	};

	if (bulletBody) {
		clearDynamicColliderRef(boxCollider);
		clearDynamicColliderRef(sphereCollider);
		clearDynamicColliderRef(capsuleCollider);
		rigidBody->ClearBulletRigidBody();
		rigidBody->SetPhysicsWorld(nullptr);
	} else if (rigidBody) {
		rigidBody->SetPhysicsWorld(nullptr);
	}

	auto detachStaticCollider = [](auto* collider) {
		if (!collider || !collider->GetBulletCollisionObject()) {
			return;
		}

		if (Physics::PhysicsWorld* world = collider->GetPhysicsWorld()) {
			world->RemoveCollisionObject(collider->GetBulletCollisionObject());
		}

		collider->SetPhysicsWorld(nullptr);
		collider->SetBulletCollisionObject(nullptr, false);
	};

	detachStaticCollider(boxCollider);
	detachStaticCollider(sphereCollider);
	detachStaticCollider(capsuleCollider);
}

void RTBEngine::Core::Application::DetachPhysicsHierarchy(ECS::GameObject* root)
{
	if (!root) {
		return;
	}

	std::vector<ECS::GameObject*> hierarchy;
	CollectHierarchy(root, hierarchy);

	for (ECS::GameObject* gameObject : hierarchy) {
		DetachPhysicsFromGameObject(gameObject);
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
