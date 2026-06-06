#include "Scene.h"
#include <algorithm>
#include <unordered_set>

#include "GameObject.h"
#include "MeshRenderer.h"
#include "CameraComponent.h"
#include "TrailRenderer.h"
#include "../Core/Logger.h"

namespace {
	RTBEngine::ECS::CameraComponent* FindCameraComponent(
		const std::vector<std::unique_ptr<RTBEngine::ECS::GameObject>>& objects,
		bool requireActive,
		bool requireMain)
	{
		for (const auto& gameObject : objects) {
			if (!gameObject) {
				continue;
			}

			if (requireActive && !gameObject->IsActive()) {
				continue;
			}

			RTBEngine::ECS::CameraComponent* camera = gameObject->GetComponent<RTBEngine::ECS::CameraComponent>();
			if (!camera || !camera->GetCamera()) {
				continue;
			}

			if (requireMain && !camera->IsMain()) {
				continue;
			}

			return camera;
		}

		return nullptr;
	}

	void SetMainCameraFlag(
		std::vector<std::unique_ptr<RTBEngine::ECS::GameObject>>& objects,
		RTBEngine::ECS::CameraComponent* selectedCamera)
	{
		for (const auto& gameObject : objects) {
			if (!gameObject) {
				continue;
			}

			RTBEngine::ECS::CameraComponent* camera = gameObject->GetComponent<RTBEngine::ECS::CameraComponent>();
			if (camera) {
				camera->SetAsMain(camera == selectedCamera);
			}
		}
	}

	void CollectHierarchyPostOrder(
		RTBEngine::ECS::GameObject* root,
		std::vector<RTBEngine::ECS::GameObject*>& outHierarchy)
	{
		if (!root) {
			return;
		}

		for (RTBEngine::ECS::GameObject* child : root->GetChildren()) {
			CollectHierarchyPostOrder(child, outHierarchy);
		}

		outHierarchy.push_back(root);
	}

	void DestroyOwnedGameObject(
		std::vector<std::unique_ptr<RTBEngine::ECS::GameObject>>& objects,
		RTBEngine::ECS::GameObject* target)
	{
		auto it = std::find_if(objects.begin(), objects.end(),
			[target](const std::unique_ptr<RTBEngine::ECS::GameObject>& obj) {
				return obj.get() == target;
			});

		if (it != objects.end()) {
			objects.erase(it);
		}
	}

	bool HasQueuedAncestor(
		RTBEngine::ECS::GameObject* candidate,
		const std::unordered_set<RTBEngine::ECS::GameObject*>& queuedRemovals)
	{
		for (RTBEngine::ECS::GameObject* parent = candidate ? candidate->GetParent() : nullptr;
			parent;
			parent = parent->GetParent()) {
			if (queuedRemovals.find(parent) != queuedRemovals.end()) {
				return true;
			}
		}

		return false;
	}
}

RTBEngine::ECS::Scene::Scene(const std::string& name) : name(name)
{
}

RTBEngine::ECS::Scene::~Scene()
{
	// Destroy leaf nodes first so parent OnDestroy() does not touch freed children.
	while (!gameObjects.empty()) {
		size_t leafIndex = gameObjects.size();

		for (size_t i = 0; i < gameObjects.size(); ++i) {
			GameObject* candidate = gameObjects[i].get();
			if (!candidate) {
				leafIndex = i;
				break;
			}

			bool hasChildInScene = false;
			for (GameObject* child : candidate->GetChildren()) {
				if (!child) {
					continue;
				}

				const auto childIt = std::find_if(
					gameObjects.begin(),
					gameObjects.end(),
					[child](const std::unique_ptr<GameObject>& obj) {
						return obj.get() == child;
					});

				if (childIt != gameObjects.end()) {
					hasChildInScene = true;
					break;
				}
			}

			if (!hasChildInScene) {
				leafIndex = i;
				break;
			}
		}

		if (leafIndex >= gameObjects.size()) {
			gameObjects.clear();
			break;
		}

		gameObjects.erase(gameObjects.begin() + static_cast<std::ptrdiff_t>(leafIndex));
	}

	pendingAdds.clear();
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
	if (!gameObject) {
		return;
	}

	if (iterationDepth > 0) {
		pendingRemoves.push_back(gameObject);
		return;
	}

	std::vector<GameObject*> hierarchy;
	CollectHierarchyPostOrder(gameObject, hierarchy);

	if (hierarchy.empty()) {
		return;
	}

	const std::unordered_set<GameObject*> hierarchySet(hierarchy.begin(), hierarchy.end());

	pendingRemoves.erase(
		std::remove_if(
			pendingRemoves.begin(),
			pendingRemoves.end(),
			[&hierarchySet](GameObject* queued) {
				return queued && hierarchySet.find(queued) != hierarchySet.end();
			}),
		pendingRemoves.end());

	for (GameObject* node : hierarchy) {
		if (node && node->GetParent()) {
			node->SetParent(nullptr);
		}
	}

	for (GameObject* node : hierarchy) {
		DestroyOwnedGameObject(gameObjects, node);
		DestroyOwnedGameObject(pendingAdds, node);
	}
}

void RTBEngine::ECS::Scene::FlushPendingCommands()
{
	if (iterationDepth > 0) return;

	// Process removes first to avoid updating objects marked for deletion.
	if (!pendingRemoves.empty()) {
		std::unordered_set<GameObject*> queuedRemovals;
		for (GameObject* target : pendingRemoves) {
			if (target) {
				queuedRemovals.insert(target);
			}
		}

		std::vector<GameObject*> rootRemovals;
		rootRemovals.reserve(pendingRemoves.size());

		for (GameObject* target : pendingRemoves) {
			if (!target) {
				continue;
			}

			if (HasQueuedAncestor(target, queuedRemovals)) {
				continue;
			}

			if (std::find(rootRemovals.begin(), rootRemovals.end(), target) == rootRemovals.end()) {
				rootRemovals.push_back(target);
			}
		}

		pendingRemoves.clear();

		for (GameObject* target : rootRemovals) {
			RemoveGameObject(target);
		}
	}

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

void RTBEngine::ECS::Scene::LateUpdate(float deltaTime)
{
	++iterationDepth;
	for (auto& gameObject : gameObjects) {
		if (gameObject) gameObject->LateUpdate(deltaTime);
	}
	--iterationDepth;
	FlushPendingCommands();
}

void RTBEngine::ECS::Scene::Render(Rendering::Camera* camera)
{
	if (!camera) return;

	CollectLights();

	const Rendering::Frustum& frustum = camera->GetFrustum();

	++iterationDepth;
	for (auto& gameObject : gameObjects) {
		if (gameObject && gameObject->IsActive()) {
			MeshRenderer* renderer = gameObject->GetComponent<MeshRenderer>();
			if (renderer && renderer->IsEnabled()) {
				// Frustum culling
				Math::Vector3 localMin, localMax;
				renderer->GetCombinedAABB(localMin, localMax);

				// Skip culling if no mesh (let renderer handle it)
				if (localMin != localMax) {
					Math::Vector3 worldMin, worldMax;
					Rendering::Frustum::TransformAABB(
						gameObject->GetWorldMatrix(), localMin, localMax, worldMin, worldMax);

					if (!frustum.IsAABBVisible(worldMin, worldMax)) {
						MeshRenderer::IncrementCulledCount();
						continue;
					}
				}

				renderer->Render(camera, lights);
			}
		}
	}

	for (auto& gameObject : gameObjects) {
		if (gameObject && gameObject->IsActive()) {
			TrailRenderer* trailRenderer = gameObject->GetComponent<TrailRenderer>();
			if (trailRenderer && trailRenderer->IsEnabled()) {
				trailRenderer->Render(camera);
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
	SetMainCameraFlag(gameObjects, camera);
	SetMainCameraFlag(pendingAdds, camera);
}

RTBEngine::ECS::CameraComponent* RTBEngine::ECS::Scene::GetMainCamera() const {
	RTBEngine::ECS::CameraComponent* camera = FindCameraComponent(gameObjects, false, true);
	if (camera) {
		return camera;
	}

	return FindCameraComponent(pendingAdds, false, true);
}

RTBEngine::Rendering::Camera* RTBEngine::ECS::Scene::GetActiveCamera() {
	RTBEngine::ECS::CameraComponent* camera = FindCameraComponent(gameObjects, true, true);
	if (!camera) {
		camera = FindCameraComponent(pendingAdds, true, true);
	}

	if (!camera) {
		camera = FindCameraComponent(gameObjects, true, false);
	}

	if (!camera) {
		camera = FindCameraComponent(pendingAdds, true, false);
	}

	return camera ? camera->GetCamera() : nullptr;
}
