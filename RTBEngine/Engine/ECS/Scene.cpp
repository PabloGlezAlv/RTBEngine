#include "Scene.h"
#include <algorithm>

#include "GameObject.h"
#include "MeshRenderer.h"
#include "CameraComponent.h"
#include "../Core/Logger.h"

RTBEngine::ECS::Scene::Scene(const std::string& name) : name(name)
{
}

RTBEngine::ECS::Scene::~Scene()
{
	gameObjects.clear();
}

void RTBEngine::ECS::Scene::AddGameObject(GameObject* gameObject)
{
	if (iterationDepth > 0) {
		pendingAdds.push_back(std::unique_ptr<GameObject>(gameObject));
	} else {
		gameObjects.push_back(std::unique_ptr<GameObject>(gameObject));
	}
	pendingRenderLog = true;
}

void RTBEngine::ECS::Scene::RemoveGameObject(GameObject* gameObject)
{
	if (iterationDepth > 0) {
		pendingRemoves.push_back(gameObject);
		return;
	}

	auto it = std::find_if(gameObjects.begin(), gameObjects.end(),
		[gameObject](const std::unique_ptr<GameObject>& obj) {
			return obj.get() == gameObject;
		});

	if (it != gameObjects.end()) {
		gameObjects.erase(it);
	}
}

void RTBEngine::ECS::Scene::FlushPendingCommands()
{
	if (iterationDepth > 0) return;

	// Process removes first to avoid updating objects marked for deletion
	for (auto* target : pendingRemoves) {
		auto it = std::find_if(gameObjects.begin(), gameObjects.end(),
			[target](const std::unique_ptr<GameObject>& obj) {
				return obj.get() == target;
			});
		if (it != gameObjects.end()) {
			gameObjects.erase(it);
		}
	}
	pendingRemoves.clear();

	// Then flush adds
	for (auto& go : pendingAdds) {
		gameObjects.push_back(std::move(go));
	}
	pendingAdds.clear();
}

RTBEngine::ECS::GameObject* RTBEngine::ECS::Scene::FindGameObject(const std::string& name)
{
	for (auto& obj : gameObjects) {
		if (obj && obj->GetName() == name) return obj.get();
	}
	for (auto& obj : pendingAdds) {
		if (obj && obj->GetName() == name) return obj.get();
	}
	return nullptr;
}

RTBEngine::ECS::GameObject* RTBEngine::ECS::Scene::FindGameObjectByUUID(const std::string& uuid)
{
	for (auto& obj : gameObjects) {
		if (obj && obj->GetUUID() == uuid) return obj.get();
	}
	for (auto& obj : pendingAdds) {
		if (obj && obj->GetUUID() == uuid) return obj.get();
	}
	return nullptr;
}

void RTBEngine::ECS::Scene::Update(float deltaTime)
{
	++iterationDepth;
	for (auto& gameObject : gameObjects) {
		if (gameObject) gameObject->Update(deltaTime);
	}
	--iterationDepth;
	FlushPendingCommands();
}

void RTBEngine::ECS::Scene::FixedUpdate(float fixedDeltaTime)
{
	++iterationDepth;
	for (auto& gameObject : gameObjects) {
		if (gameObject) gameObject->FixedUpdate(fixedDeltaTime);
	}
	--iterationDepth;
	FlushPendingCommands();
}

void RTBEngine::ECS::Scene::Render(Rendering::Camera* camera)
{
	if (!camera) return;

	CollectLights();

	++iterationDepth;
	for (auto& gameObject : gameObjects) {
		if (gameObject && gameObject->IsActive()) {
			MeshRenderer* renderer = gameObject->GetComponent<MeshRenderer>();
			if (renderer && renderer->IsEnabled()) {
				renderer->Render(camera, lights);
			}
		}
	}
	--iterationDepth;
	FlushPendingCommands();
}

void RTBEngine::ECS::Scene::SetSkyboxCubemap(Rendering::Cubemap* cubemap) {
	skyboxCubemap = cubemap;
}

uint32_t RTBEngine::ECS::Scene::GetActiveGameObjectCount() const {
	uint32_t count = 0;
	for (const auto& go : gameObjects) {
		if (go && go->IsActive()) count++;
	}
	return count;
}

uint32_t RTBEngine::ECS::Scene::GetActiveComponentCount() const {
	uint32_t count = 0;
	for (const auto& go : gameObjects) {
		if (go && go->IsActive()) {
			count += static_cast<uint32_t>(go->GetComponents().size());
		}
	}
	return count;
}

void RTBEngine::ECS::Scene::CollectLights()
{
	lights.clear();

	for (auto& gameObject : gameObjects) {
		if (gameObject && gameObject->IsActive()) {
			LightComponent* lightComp = gameObject->GetComponent<LightComponent>();
			if (lightComp && lightComp->IsEnabled()) {
				lights.push_back(lightComp->GetLight());
			}
		}
	}
}

void RTBEngine::ECS::Scene::SetMainCamera(CameraComponent* camera) {
	mainCamera = camera;
}

RTBEngine::ECS::CameraComponent* RTBEngine::ECS::Scene::GetMainCamera() const {
	return mainCamera;
}

RTBEngine::Rendering::Camera* RTBEngine::ECS::Scene::GetActiveCamera() {
	// If main camera is set, use it
	if (mainCamera && mainCamera->GetCamera()) {
		return mainCamera->GetCamera();
	}

	// Otherwise, find first CameraComponent marked as main
	for (auto& go : gameObjects) {
		if (go && go->IsActive()) {
			CameraComponent* camComp = go->GetComponent<CameraComponent>();
			if (camComp && camComp->IsMain() && camComp->GetCamera()) {
				mainCamera = camComp;
				return camComp->GetCamera();
			}
		}
	}

	// If no main camera, find any CameraComponent
	for (auto& go : gameObjects) {
		if (go && go->IsActive()) {
			CameraComponent* camComp = go->GetComponent<CameraComponent>();
			if (camComp && camComp->GetCamera()) {
				mainCamera = camComp;
				return camComp->GetCamera();
			}
		}
	}

	return nullptr;
}
