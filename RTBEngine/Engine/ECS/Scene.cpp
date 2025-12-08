#include "Scene.h"
#include <algorithm>

#include "GameObject.h"
#include "MeshRenderer.h"

RTBEngine::ECS::Scene::Scene(const std::string& name) : name(name)
{
	gameObjects = std::vector<GameObject*>();
}

RTBEngine::ECS::Scene::~Scene()
{
	for (GameObject* obj : gameObjects) {
		delete obj;
	}
	gameObjects.clear();
}

void RTBEngine::ECS::Scene::AddGameObject(GameObject* gameObject)
{
	gameObjects.push_back(gameObject);
}

void RTBEngine::ECS::Scene::RemoveGameObject(GameObject* gameObject)
{
	auto it = std::find(gameObjects.begin(), gameObjects.end(), gameObject);
	if (it != gameObjects.end()) {
		gameObjects.erase(it);
	}
}

RTBEngine::ECS::GameObject* RTBEngine::ECS::Scene::FindGameObject(const std::string& name)
{
	auto it = std::find_if(gameObjects.begin(), gameObjects.end(),
		[&name](GameObject* obj) { return obj->GetName() == name; });

	if (it != gameObjects.end()) {
        return *it; 
    }

	return nullptr;
}

void RTBEngine::ECS::Scene::Update(float deltaTime)
{
	for (auto& gameObject : gameObjects) {
		gameObject->Update(deltaTime);
	}
}

void RTBEngine::ECS::Scene::Render(Rendering::Camera* camera)
{
	if (!camera) return;

	for (auto& gameObject : gameObjects) {
		if (gameObject->IsActive()) {
			MeshRenderer* renderer = gameObject->GetComponent<MeshRenderer>();
			if (renderer && renderer->IsEnabled()) {
				renderer->Render(camera);
			}
		}
	}
}
