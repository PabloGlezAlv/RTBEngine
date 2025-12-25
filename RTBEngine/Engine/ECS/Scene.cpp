#include "Scene.h"
#include <algorithm>

#include "GameObject.h"
#include "MeshRenderer.h"
#include "CameraComponent.h"

RTBEngine::ECS::Scene::Scene(const std::string& name) : name(name)
{
}

RTBEngine::ECS::Scene::~Scene()
{
	gameObjects.clear();
}

void RTBEngine::ECS::Scene::AddGameObject(GameObject* gameObject)
{
	gameObjects.push_back(std::unique_ptr<GameObject>(gameObject));
}

void RTBEngine::ECS::Scene::RemoveGameObject(GameObject* gameObject)
{
	auto it = std::find_if(gameObjects.begin(), gameObjects.end(),
		[gameObject](const std::unique_ptr<GameObject>& obj) {
			return obj.get() == gameObject;
		});

	if (it != gameObjects.end()) {
		gameObjects.erase(it);
	}
}

RTBEngine::ECS::GameObject* RTBEngine::ECS::Scene::FindGameObject(const std::string& name)
{
	auto it = std::find_if(gameObjects.begin(), gameObjects.end(),
		[&name](const std::unique_ptr<GameObject>& obj) {
			return obj->GetName() == name;
		});

	if (it != gameObjects.end()) {
        return it->get();
    }

	return nullptr;
}

void RTBEngine::ECS::Scene::Update(float deltaTime)
{
	for (auto& gameObject : gameObjects) {
		gameObject->Update(deltaTime);
	}
}

void RTBEngine::ECS::Scene::FixedUpdate(float fixedDeltaTime)
{
    for (auto& gameObject : gameObjects) {
        gameObject->FixedUpdate(fixedDeltaTime);
    }
}

void RTBEngine::ECS::Scene::Render(Rendering::Camera* camera)
{
	if (!camera) return;

	CollectLights();

	for (auto& gameObject : gameObjects) {
		if (gameObject->IsActive()) {
			MeshRenderer* renderer = gameObject->GetComponent<MeshRenderer>();
			if (renderer && renderer->IsEnabled()) {
				renderer->Render(camera, lights);
			}
		}
	}
}

void RTBEngine::ECS::Scene::CollectLights()
{
	lights.clear();

	for (auto& gameObject : gameObjects) {
		if (gameObject->IsActive()) {
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
		if (go->IsActive()) {
			CameraComponent* camComp = go->GetComponent<CameraComponent>();
			if (camComp && camComp->IsMain() && camComp->GetCamera()) {
				mainCamera = camComp;
				return camComp->GetCamera();
			}
		}
	}

	// If no main camera, find any CameraComponent
	for (auto& go : gameObjects) {
		if (go->IsActive()) {
			CameraComponent* camComp = go->GetComponent<CameraComponent>();
			if (camComp && camComp->GetCamera()) {
				mainCamera = camComp;
				return camComp->GetCamera();
			}
		}
	}

	return nullptr;
}

