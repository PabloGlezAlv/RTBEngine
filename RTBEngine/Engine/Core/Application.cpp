#include "Application.h"
#include "Window.h"
#include "Time.h"
#include "Scheduler.h"
#include "../Input/Input.h"
#include "../Rendering/RHI/RenderDevice.h"
#include "../Rendering/RHI/RenderDeviceFactory.h"
#include "../Rendering/RHI/GraphicsAPI.h"
#include "../Rendering/Rendering.h"
#include "../Rendering/Texture.h"
#include "../Scene/Scene.h"
#include "../Scene/GameObject.h"
#include "../Scene/LightComponent.h"
#include "../Scene/RigidBodyComponent.h"
#include "../Scene/CameraComponent.h"
#include "../Scene/BoxColliderComponent.h"
#include "../Scene/SphereColliderComponent.h"
#include "../Scene/CapsuleColliderComponent.h"
#include "../Scene/MeshDrawSubmit.h"
#include "../Scene/MeshRenderer.h"
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
#include "../Scene/PrefabRegistry.h"
#include "../Scene/SceneManager.h"
#include "../Rendering/Skybox.h"
#include "../ECS/World.h"
#include "../ECS/EcsStats.h"
#include "../Rendering/Cubemap.h"
#include "../Rendering/Lighting/LightingUBO.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Lighting/DirectionalLight.h"
#include "../Rendering/GI/DDGISystem.h"
#include "../Rendering/Lighting/LightingProjectSettings.h"
#include "../Rendering/FogUniforms.h"
#include "../Rendering/PostProcess/VolumeStack.h"
#include "../Rendering/VolumetricFogPass.h"

#include "../Rendering/Frustum.h"
#include "../Online/OnlineSystem.h"
#include "../Math/Matrix/Matrix4.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <iostream>
#include <filesystem>
#include <atomic>
#include <algorithm>
#include <vector>
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

	void CollectHierarchy(RTBEngine::Scene::GameObject* root,
		std::vector<RTBEngine::Scene::GameObject*>& outHierarchy)
	{
		if (!root) {
			return;
		}

		outHierarchy.push_back(root);
		for (RTBEngine::Scene::GameObject* child : root->GetChildren()) {
			CollectHierarchy(child, outHierarchy);
		}
	}

	// One (mesh, renderer) pair queued for the depth-only pass so it can be
	// grouped by mesh and drawn with instancing when possible.
	struct ShadowDraw {
		RTBEngine::Rendering::Mesh* mesh = nullptr;
		RTBEngine::Scene::MeshRenderer* renderer = nullptr;
	};

	// Reused across shadow passes to avoid per-frame heap allocations.
	thread_local std::vector<ShadowDraw> g_shadowDrawScratch;
	thread_local std::vector<RTBEngine::Math::Matrix4> g_shadowInstanceScratch;

	bool ShadowDrawIsSkinned(RTBEngine::Scene::MeshRenderer* renderer)
	{
		if (!renderer) {
			return false;
		}
		RTBEngine::Animation::Animator* animator = renderer->GetActiveAnimator();
		return animator && animator->ShouldSkinMesh();
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

	auto& device = Rendering::RHI::RenderDevice::Get();
	if (!device.InitializeImGuiBackend(window->GetSDLWindow())) {
		RTB_ERROR("Application::InitializeImGui - Failed to initialize ImGui render backend");
		ImGui::DestroyContext();
		return false;
	}

	imguiInitialized = true;
	return true;
}

 void* RTBEngine::Core::Application::GetImGuiContext()
 {
 	return ImGui::GetCurrentContext();
 }

void RTBEngine::Core::Application::ShutdownImGui()
{
	if (ImGui::GetCurrentContext() == nullptr) {
		imguiInitialized = false;
		return;
	}

	if (imguiInitialized) {
		// Ensure the main GL context is current before releasing ImGui GL resources.
		// ViewportsEnable can leave a different context active after rendering.
		if (Rendering::RHI::RenderDevice::HasDevice()) {
			auto& device = Rendering::RHI::RenderDevice::Get();
			device.MakeCurrent();
			device.ShutdownImGuiBackend();
		}
	}

	ImGui::DestroyContext();
	imguiInitialized = false;
}

bool RTBEngine::Core::Application::Initialize()
{
	ClearQuitRequest();

	window = std::make_unique<Window>(config.window.title, config.window.width, config.window.height, config.window.fullscreen, config.window.maximized);
	if (!window->Initialize(config.rendering.graphicsAPI)) {
		RTB_ERROR("Failed to initialize Window");
		return false;
	}

	{
		auto device = Rendering::RHI::RenderDeviceFactory::Create(config.rendering.graphicsAPI);
		if (!device || !device->Initialize(window->GetSDLWindow(), config.window.vSync)) {
			RTB_ERROR("Failed to initialize RenderDevice");
			return false;
		}
		Rendering::RHI::RenderDevice::Set(std::move(device));
		Rendering::Texture::SetGpuDestroyEnabled(true);
		RTB_INFO(std::string("RenderDevice ready: ")
			+ Rendering::RHI::GraphicsAPIToString(Rendering::RHI::RenderDevice::Get().GetAPI()));
		Rendering::GI::DDGISystem::GetInstance().Initialize();
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
		Scene::PrefabRegistry::GetInstance().Clear();
		if (fs::exists(assetsPath) && fs::is_directory(assetsPath)) {
			Scene::PrefabRegistry::GetInstance().LoadAll(assetsPath.string());
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

	Rendering::Shader* volumetricFogShader = resources.LoadShader(
		"volumetric_fog",
		"Default/Shaders/volumetric_fog.vert",
		"Default/Shaders/volumetric_fog.frag"
	);
	if (!volumetricFogShader) {
		RTB_WARN("Failed to load volumetric fog shader (volumetric pass disabled)");
	} else if (!Rendering::VolumetricFogPass::GetInstance().Initialize(volumetricFogShader)) {
		RTB_WARN("Failed to initialize volumetric fog pass");
	}

	if (!config.initialScenePath.empty()) {
		namespace fs = std::filesystem;
		const fs::path assetsPath = fs::current_path() / "Assets";
		if (fs::exists(assetsPath) && fs::is_directory(assetsPath)) {
			resources.ScanShaderAssets(assetsPath);
		}
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

	// Load fonts after ImGui context is ready (skipped on Vulkan MVP without ImGui).
	if (imguiInitialized) {
		resources.GetDefaultFont();
	}

	RTB_INFO("RTBEngine Initialized Successfully");

	Scene::SceneManager& sceneMgr = Scene::SceneManager::GetInstance();
	sceneMgr.Initialize();
	sceneMgr.SetOnHierarchyAdded([this](Scene::GameObject* root) {
		InitializePhysicsForHierarchy(root);
	});
	sceneMgr.SetOnHierarchyDeactivated([this](Scene::GameObject* root) {
		DetachPhysicsHierarchy(root);
	});

	sceneMgr.SetOnSceneUnloading([this](Scene::Scene* /*scene*/) {
		UI::CanvasSystem::GetInstance().ClearState();
		if (ecsWorld) {
			ecsWorld->Clear();
		}
		// Remove all Bullet objects from the world BEFORE GameObjects are destroyed.
		// This prevents btDbvtBroadphase::destroyProxy from accessing a freed proxy
		// when ~BoxColliderComponent() deletes the raw btCollisionObject.
		ResetPhysics();
	});

	sceneMgr.SetOnSceneLoaded([this](Scene::Scene* scene) {
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

	ecsWorld = new ECS::World();
	ECS::World::SetActive(ecsWorld);
	Scripting::ScriptManager::GetInstance().InitializeGameEcs(*ecsWorld);

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
	Scene::SceneManager::GetInstance().Shutdown();

	if (ecsWorld) {
		ECS::World::SetActive(nullptr);
		delete ecsWorld;
		ecsWorld = nullptr;
	}

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
	// UnloadScripts() also evicts script-owned DataAssets while the DLL is still mapped.
	Scripting::ScriptManager::GetInstance().UnloadScripts();

	ResourceManager::GetInstance().Clear();

	Audio::AudioSystem::GetInstance().Shutdown();

	Rendering::GI::DDGISystem::GetInstance().Shutdown();
	Rendering::VolumetricFogPass::GetInstance().Shutdown();
	// Scene is already unloaded; prevent any late Texture dtors (static teardown) from
	// touching Vulkan after the device/layers are destroyed.
	Rendering::Texture::SetGpuDestroyEnabled(false);
	Rendering::RHI::RenderDevice::Shutdown();
	window.reset();
}

void RTBEngine::Core::Application::ProcessInput()
{
	Input::InputManager& input = Input::InputManager::GetInstance();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (imguiInitialized && !(IsMouseOwnedByGameplay(window.get()) && IsMouseUiEvent(event))) {
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
	Scheduler::GetInstance().Tick(Time::GetDeltaTime(), Time::GetUnscaledDeltaTime());

	Online::OnlineSystem::GetInstance().Tick(deltaTime);

	Scene::SceneManager& sceneMgr = Scene::SceneManager::GetInstance();
	sceneMgr.ProcessPendingSceneLoad();

	if (IsQuitRequested()) {
		return;
	}

	Scene::Scene* scene = sceneMgr.GetActiveScene();
	if (!scene) return;

	if (physicsSystem) {
		physicsSystem->SyncRenderTransforms(scene, 1.0f);
	}

	scene->Update(deltaTime);

	if (ecsWorld) {
		ecsWorld->Tick(ECS::SystemPhase::Simulation, deltaTime);
	}

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

	if (ecsWorld) {
		ecsWorld->Tick(ECS::SystemPhase::Presentation, deltaTime);
	}
}

void RTBEngine::Core::Application::Render()
{
	Scene::Scene* scene = Scene::SceneManager::GetInstance().GetActiveScene();
	if (!scene) return;

	Rendering::Camera* activeCamera = scene->GetActiveCamera();
	if (!activeCamera) return;

	if (Scene::CameraComponent* activeCameraComponent = scene->GetMainCamera()) {
		activeCameraComponent->SyncNow();
		activeCamera = activeCameraComponent->GetCamera();
		if (!activeCamera) {
			return;
		}
	}

	auto& canvasSystem = UI::CanvasSystem::GetInstance();
	if (imguiInitialized) {
		canvasSystem.Update(scene);
	}

	RenderShadowPass(scene);

	UploadSceneLighting(scene);
	Rendering::LightingUBO::GetInstance().Bind();
	Rendering::GI::DDGISystem::GetInstance().Update(scene);

	RenderGeometryPass(scene, activeCamera);

	if (imguiInitialized) {
		auto& device = Rendering::RHI::RenderDevice::Get();
		device.BeginImGuiFrame();
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

		ImGui::Render();
		device.QueueImGuiDrawData(ImGui::GetDrawData());
	}

	window->SwapBuffers();
}

void RTBEngine::Core::Application::RenderShadowPass(Scene::Scene* scene)
{
	const auto& lightingSettings = Rendering::LightingProjectSettings::Get();
	if (!lightingSettings.shadowsEnabled) {
		return;
	}

	Rendering::Shader* shadowShader = ResourceManager::GetInstance().GetShader("shadow");
	if (!shadowShader) return;

	shadowShader->Bind();

	const int shadowResolution = lightingSettings.GetClampedShadowMapResolution();

	for (Scene::LightComponent* lightComp : scene->GetCachedLightComponents()) {
		if (!lightComp || !lightComp->GetLight()) continue;

		Scene::GameObject* go = lightComp->GetOwner();
		if (!go || !go->IsActiveInHierarchy()) continue;

		if (lightComp->GetLight()->GetType() != Rendering::LightType::Directional) continue;
		auto* dirLight = static_cast<Rendering::DirectionalLight*>(lightComp->GetLight());
		if (!dirLight || !dirLight->GetCastShadows()) continue;

		dirLight->SetShadowMapResolution(shadowResolution);
		Rendering::ShadowMap* shadowMap = dirLight->GetShadowMap();
		if (!shadowMap) continue;

		Math::Vector3 sceneCenter(0.0f, 2.0f, 0.0f);
		float sceneRadius = 50.0f;
		Math::Matrix4 lightSpaceMatrix = dirLight->GetLightSpaceMatrix(sceneCenter, sceneRadius);

		// Frustum cull in engine/OpenGL light space; Vulkan needs a clip fix for the GPU.
		Rendering::Frustum shadowFrustum;
		shadowFrustum.ExtractPlanes(lightSpaceMatrix);
		if (Rendering::RHI::RenderDevice::Get().GetAPI() == Rendering::RHI::GraphicsAPI::Vulkan) {
			lightSpaceMatrix = Math::Matrix4::VulkanClipCorrection() * lightSpaceMatrix;
		}

		shadowShader->SetMatrix4("uLightSpaceMatrix", lightSpaceMatrix);

		shadowMap->BindForWriting();

		auto& device = Rendering::RHI::RenderDevice::Get();
		device.SetViewport(0, 0, shadowMap->GetResolution(), shadowMap->GetResolution());
		device.Clear(Rendering::RHI::ClearMask::Depth);

		// Disable culling to render all faces (fixes shadow issues with single-sided geometry)
		device.SetCullFace(false);
		RenderSceneDepthOnly(scene, shadowShader, shadowFrustum);

		device.SetCullFace(true);

		shadowMap->Unbind();
	}

	Rendering::RHI::RenderDevice::Get().SetViewport(0, 0, window->GetWidth(), window->GetHeight());
}

void RTBEngine::Core::Application::UploadSceneLighting(Scene::Scene* scene)
{
	if (!scene) return;

	std::vector<Rendering::Light*> activeLights;
	activeLights.reserve(scene->GetCachedLightComponents().size());

	for (Scene::LightComponent* lightComp : scene->GetCachedLightComponents()) {
		if (!lightComp || !lightComp->IsEnabled() || !lightComp->GetLight()) continue;

		Scene::GameObject* go = lightComp->GetOwner();
		if (!go || !go->IsActiveInHierarchy()) continue;

		activeLights.push_back(lightComp->GetLight());
	}

	Rendering::LightingUBO::GetInstance().Upload(activeLights);
}

void RTBEngine::Core::Application::RenderSceneDepthOnly(Scene::Scene* scene, Rendering::Shader* shader, const Rendering::Frustum& frustum)
{
	// 1) Collect every visible mesh into a flat list. Culling stays per-renderer
	//    (using the combined AABB), matching the previous behaviour.
	std::vector<ShadowDraw>& draws = g_shadowDrawScratch;
	draws.clear();

	for (Scene::MeshRenderer* meshRenderer : scene->GetCachedMeshRenderers()) {
		if (!meshRenderer || !meshRenderer->IsEnabled()) continue;

		Scene::GameObject* go = meshRenderer->GetOwner();
		if (!go || !go->IsActiveInHierarchy()) continue;

		// Frustum culling
		Math::Vector3 localMin, localMax;
		meshRenderer->GetCombinedAABB(localMin, localMax);
		if (localMin != localMax) {
			Math::Vector3 worldMin, worldMax;
			Rendering::Frustum::TransformAABB(go->GetWorldMatrix(), localMin, localMax, worldMin, worldMax);
			if (!frustum.IsAABBVisible(worldMin, worldMax)) continue;
		}

		if (meshRenderer->IsMultiMesh()) {
			for (auto* mesh : meshRenderer->GetMeshes()) {
				if (mesh) draws.push_back({ mesh, meshRenderer });
			}
		}
		else {
			if (auto* mesh = meshRenderer->GetMesh()) {
				draws.push_back({ mesh, meshRenderer });
			}
		}
	}

	if (draws.empty()) return;

	// 2) Group identical meshes together. Depth-only ignores material, so batching
	//    by mesh pointer alone maximises instancing opportunities.
	std::sort(draws.begin(), draws.end(), [](const ShadowDraw& a, const ShadowDraw& b) {
		return a.mesh < b.mesh;
	});

	std::vector<Math::Matrix4>& instanceMatrices = g_shadowInstanceScratch;

	const std::size_t drawCount = draws.size();
	std::size_t i = 0;
	while (i < drawCount) {
		Rendering::Mesh* mesh = draws[i].mesh;

		std::size_t j = i + 1;
		while (j < drawCount && draws[j].mesh == mesh) {
			++j;
		}
		const std::size_t runLength = j - i;

		// Instancing needs 2+ non-skinned draws sharing the same mesh.
		bool canInstance = runLength >= 2;
		if (canInstance) {
			for (std::size_t k = i; k < j; ++k) {
				if (ShadowDrawIsSkinned(draws[k].renderer)) {
					canInstance = false;
					break;
				}
			}
		}

		if (canInstance) {
			instanceMatrices.clear();
			instanceMatrices.reserve(runLength);
			for (std::size_t k = i; k < j; ++k) {
				Scene::GameObject* owner = draws[k].renderer ? draws[k].renderer->GetOwner() : nullptr;
				if (owner) {
					instanceMatrices.push_back(owner->GetWorldMatrix());
				}
			}

			if (!instanceMatrices.empty()) {
				Scene::SubmitInstancedMeshDraw(
					mesh,
					shader,
					instanceMatrices.data(),
					instanceMatrices.size());
			}
		}
		else {
			for (std::size_t k = i; k < j; ++k) {
				Scene::MeshRenderer* renderer = draws[k].renderer;
				Scene::GameObject* owner = renderer ? renderer->GetOwner() : nullptr;
				if (!owner) continue;

				shader->SetBool("uUseInstancing", false);
				shader->SetMatrix4("uModel", owner->GetWorldMatrix());

				Animation::Animator* animator = renderer->GetActiveAnimator();
				if (animator && animator->ShouldSkinMesh()) {
					shader->SetBool("uHasAnimation", true);
					animator->BindBoneMatrices();
				}
				else {
					shader->SetBool("uHasAnimation", false);
				}

				mesh->Draw();
			}
		}

		i = j;
	}
}

void RTBEngine::Core::Application::RenderGeometryPass(Scene::Scene* scene, Rendering::Camera* camera)
{
	Rendering::VolumeStack::Evaluate(camera, scene);

	auto& device = Rendering::RHI::RenderDevice::Get();
	device.SetClearColor(config.rendering.clearColorR, config.rendering.clearColorG,
		config.rendering.clearColorB, 1.0f);
	device.Clear(Rendering::RHI::ClearMask::ColorDepth);

	Rendering::Shader* shader = ResourceManager::GetInstance().GetShader("basic");
	if (!shader) return;

	shader->Bind();

	UploadSceneLighting(scene);
	Rendering::LightingUBO::GetInstance().Bind();
	Rendering::CameraUBO::GetInstance().Upload(camera);
	Rendering::CameraUBO::GetInstance().Bind();
	Rendering::FogUniforms::Apply(shader);

	std::vector<Rendering::Light*> activeLights;
	activeLights.reserve(scene->GetCachedLightComponents().size());
	Rendering::DirectionalLight* shadowCastingLight = nullptr;

	for (Scene::LightComponent* lightComp : scene->GetCachedLightComponents()) {
		if (!lightComp || !lightComp->IsEnabled() || !lightComp->GetLight()) continue;

		Scene::GameObject* go = lightComp->GetOwner();
		if (!go || !go->IsActiveInHierarchy()) continue;

		Rendering::Light* light = lightComp->GetLight();
		activeLights.push_back(light);

		if (light->GetType() == Rendering::LightType::Directional) {
			auto* dirLight = static_cast<Rendering::DirectionalLight*>(light);
			if (dirLight->GetCastShadows()
				&& Rendering::LightingProjectSettings::Get().shadowsEnabled
				&& dirLight->GetShadowMap()) {
				shadowCastingLight = dirLight;
			}
		}
	}

	(void)activeLights;

	auto* activeVolume = Rendering::GI::DDGISystem::GetInstance().GetActiveVolume();
	const bool ddgiAvailable = Rendering::GI::DDGISystem::GetInstance().IsAvailable() && activeVolume;
	if (ddgiAvailable) {
		Rendering::GI::DDGISystem::GetInstance().BindForShading();
		const bool ddgiOn = activeVolume->GetSettings().enabled && activeVolume->IsGpuReady();
		shader->SetBool("uDDGIEnabled", ddgiOn);
	} else {
		shader->SetBool("uDDGIEnabled", false);
	}

	if (shadowCastingLight) {
		shader->SetBool("uHasShadows", true);
		shader->SetFloat("uShadowBias", shadowCastingLight->GetShadowBias());

		shadowCastingLight->GetShadowMap()->BindForReading(1);
		shader->SetInt("uShadowMap", 1);

		Math::Vector3 sceneCenter(0.0f, 2.0f, 0.0f);
		float sceneRadius = 50.0f;
		Math::Matrix4 lightSpaceMatrix = shadowCastingLight->GetLightSpaceMatrix(sceneCenter, sceneRadius);
		if (Rendering::RHI::RenderDevice::Get().GetAPI() == Rendering::RHI::GraphicsAPI::Vulkan) {
			lightSpaceMatrix = Math::Matrix4::VulkanClipCorrection() * lightSpaceMatrix;
		}
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

	// World-space UI uses ImGui font atlases; skip when ImGui is not available (e.g. Vulkan MVP).
	if (imguiInitialized) {
		UI::CanvasSystem::GetInstance().RenderWorldSpace(camera);
	}
}

void RTBEngine::Core::Application::RenderVolumetricFogPass(Scene::Scene* scene,
	Rendering::Camera* camera,
	Rendering::RHI::GpuId sceneDepthTexture)
{
	if (!scene || !camera || sceneDepthTexture == Rendering::RHI::kInvalidGpuId) {
		return;
	}

	const Rendering::FogFrameState& fogState = Rendering::VolumeStack::GetCurrentFrameState();
	if (!fogState.volumetricFogEnabled) {
		return;
	}

	Rendering::DirectionalLight* shadowCastingLight = nullptr;
	for (Scene::LightComponent* lightComp : scene->GetCachedLightComponents()) {
		if (!lightComp || !lightComp->IsEnabled() || !lightComp->GetLight()) continue;
		Scene::GameObject* go = lightComp->GetOwner();
		if (!go || !go->IsActiveInHierarchy()) continue;

		Rendering::Light* light = lightComp->GetLight();
		if (light->GetType() == Rendering::LightType::Directional) {
			auto* dirLight = static_cast<Rendering::DirectionalLight*>(light);
			if (dirLight->GetCastShadows()
				&& Rendering::LightingProjectSettings::Get().shadowsEnabled
				&& dirLight->GetShadowMap()) {
				shadowCastingLight = dirLight;
				break;
			}
		}
	}

	Math::Matrix4 lightSpaceMatrix = Math::Matrix4::Identity();
	if (shadowCastingLight) {
		Math::Vector3 sceneCenter(0.0f, 2.0f, 0.0f);
		float sceneRadius = 50.0f;
		lightSpaceMatrix = shadowCastingLight->GetLightSpaceMatrix(sceneCenter, sceneRadius);
		if (Rendering::RHI::RenderDevice::Get().GetAPI() == Rendering::RHI::GraphicsAPI::Vulkan) {
			lightSpaceMatrix = Math::Matrix4::VulkanClipCorrection() * lightSpaceMatrix;
		}
	}

	Rendering::VolumetricFogPass::GetInstance().Render(
		camera,
		sceneDepthTexture,
		shadowCastingLight,
		lightSpaceMatrix);
}

void RTBEngine::Core::Application::ResetPhysics()
{
	if (physicsWorld)
		physicsWorld->ResetObjects();

	if (physicsSystem)
		physicsSystem->Reset();

}

const RTBEngine::ECS::EcsSimulationStats& RTBEngine::Core::Application::GetEcsSimulationStats() const
{
	static const RTBEngine::ECS::EcsSimulationStats kEmptyStats{};
	return ecsWorld ? ecsWorld->GetSimulationStats() : kEmptyStats;
}

void RTBEngine::Core::Application::RebuildPhysicsForScene(Scene::Scene* scene)
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

void RTBEngine::Core::Application::InitializePhysicsForGameObject(Scene::GameObject* gameObject)
{
	if (!gameObject || !physicsSystem) {
		return;
	}

	Scene::BoxColliderComponent* boxCollider = gameObject->GetComponent<Scene::BoxColliderComponent>();
	if (boxCollider) {
		if (!boxCollider->GetBulletCollisionObject() && !boxCollider->GetPhysicsWorld()) {
			physicsSystem->InitializeCollider(gameObject, boxCollider);
		}
		return;
	}

	Scene::SphereColliderComponent* sphereCollider = gameObject->GetComponent<Scene::SphereColliderComponent>();
	if (sphereCollider) {
		if (!sphereCollider->GetBulletCollisionObject() && !sphereCollider->GetPhysicsWorld()) {
			physicsSystem->InitializeCollider(gameObject, sphereCollider);
		}
		return;
	}

	Scene::CapsuleColliderComponent* capsuleCollider = gameObject->GetComponent<Scene::CapsuleColliderComponent>();
	if (capsuleCollider &&
		!capsuleCollider->GetBulletCollisionObject() &&
		!capsuleCollider->GetPhysicsWorld()) {
		physicsSystem->InitializeCollider(gameObject, capsuleCollider);
	}
}

void RTBEngine::Core::Application::InitializePhysicsForScene(Scene::Scene* scene)
{
	if (!scene || !physicsSystem)
		return;

	for (const auto& go : scene->GetGameObjects())
	{
		InitializePhysicsForGameObject(go.get());
	}
}

void RTBEngine::Core::Application::InitializePhysicsForHierarchy(Scene::GameObject* root)
{
	if (!root) {
		return;
	}

	std::vector<Scene::GameObject*> hierarchy;
	CollectHierarchy(root, hierarchy);

	for (Scene::GameObject* gameObject : hierarchy) {
		InitializePhysicsForGameObject(gameObject);
	}
}

void RTBEngine::Core::Application::DetachPhysicsFromGameObject(Scene::GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	Scene::BoxColliderComponent* boxCollider = gameObject->GetComponent<Scene::BoxColliderComponent>();
	Scene::SphereColliderComponent* sphereCollider = gameObject->GetComponent<Scene::SphereColliderComponent>();
	Scene::CapsuleColliderComponent* capsuleCollider = gameObject->GetComponent<Scene::CapsuleColliderComponent>();
	Scene::RigidBodyComponent* rbComp = gameObject->GetComponent<Scene::RigidBodyComponent>();
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

void RTBEngine::Core::Application::DetachPhysicsHierarchy(Scene::GameObject* root)
{
	if (!root) {
		return;
	}

	std::vector<Scene::GameObject*> hierarchy;
	CollectHierarchy(root, hierarchy);

	for (Scene::GameObject* gameObject : hierarchy) {
		DetachPhysicsFromGameObject(gameObject);
	}
}

void RTBEngine::Core::Application::OnWindowResized(int width, int height)
{
	if (Rendering::RHI::RenderDevice::HasDevice()) {
		Rendering::RHI::RenderDevice::Get().SetViewport(0, 0, width, height);
	}

	Scene::Scene* scene = Scene::SceneManager::GetInstance().GetActiveScene();
	if (scene && scene->GetActiveCamera()) {
		float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		scene->GetActiveCamera()->SetAspectRatio(aspectRatio);
	}
}
