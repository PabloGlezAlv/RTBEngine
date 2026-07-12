#include "Scene.h"
#include "SceneLifecycle.h"
#include "../Navigation/NavPathService.h"
#include <algorithm>
#include <functional>
#include <unordered_set>

#include "GameObject.h"
#include "MeshRenderer.h"
#include "../Animation/Animator.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "RigidBodyComponent.h"
#include "Occludable.h"
#include "OcclusionTarget.h"
#include "TrailRenderer.h"
#include "ParticleSystem.h"
#include "../UI/Canvas.h"
#include "../Core/Logger.h"
#include "../Core/TypeId.h"
#include "../Rendering/CameraUBO.h"
#include "../Rendering/Lighting/LightingUBO.h"
#include "../Rendering/Material.h"
#include "../Rendering/Shader.h"
#include "../Rendering/ShaderProperties.h"
#include "../Rendering/Texture.h"

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

			if (requireActive && !gameObject->IsActiveInHierarchy()) {
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

	bool ContainsGameObject(
		const std::vector<std::unique_ptr<RTBEngine::ECS::GameObject>>& objects,
		RTBEngine::ECS::GameObject* target)
	{
		if (!target) {
			return false;
		}

		return std::any_of(objects.begin(), objects.end(),
			[target](const std::unique_ptr<RTBEngine::ECS::GameObject>& obj) {
				return obj.get() == target;
			});
	}

	void PrunePendingLifecycleRoots(
		std::vector<RTBEngine::ECS::GameObject*>& pendingLifecycleRoots,
		const std::unordered_set<RTBEngine::ECS::GameObject*>& removed)
	{
		if (removed.empty() || pendingLifecycleRoots.empty()) {
			return;
		}

		pendingLifecycleRoots.erase(
			std::remove_if(
				pendingLifecycleRoots.begin(),
				pendingLifecycleRoots.end(),
				[&removed](RTBEngine::ECS::GameObject* root) {
					return !root || removed.find(root) != removed.end();
				}),
			pendingLifecycleRoots.end());
	}

	struct OpaqueMeshDraw {
		RTBEngine::ECS::MeshRenderer* renderer = nullptr;
		RTBEngine::Rendering::Mesh* mesh = nullptr;
		RTBEngine::Rendering::Material* material = nullptr;
	};

	struct OpaqueRenderBatchState {
		RTBEngine::Rendering::Shader* shader = nullptr;
		RTBEngine::Rendering::Texture* texture = nullptr;
		RTBEngine::Rendering::Material* material = nullptr;
		float lastAlphaMultiplier = 1.0f;
	};

	// Reusable per-frame scratch buffers for the render passes. Rendering happens on the main
	// thread; keeping these persistent avoids a heap allocation per frame (and per renderer).
	thread_local std::vector<OpaqueMeshDraw> g_opaqueDrawScratch;
	thread_local std::vector<OpaqueMeshDraw> g_transparentDrawScratch;
	thread_local std::vector<OpaqueMeshDraw> g_sortedTransparentScratch;
	thread_local std::vector<OpaqueMeshDraw> g_discardDrawScratch;
	// Per-instance model matrices reused each frame for instanced opaque batches.
	thread_local std::vector<RTBEngine::Math::Matrix4> g_instanceMatrixScratch;

	// A draw is skinned when its renderer is bound to an animator currently posing bones.
	// Skinned meshes cannot be instanced (each needs its own model/bone matrices).
	bool DrawIsSkinned(RTBEngine::ECS::MeshRenderer* renderer)
	{
		if (!renderer) {
			return false;
		}
		RTBEngine::Animation::Animator* animator = renderer->GetActiveAnimator();
		return animator && animator->ShouldSkinMesh();
	}

	void ApplyOpaqueDrawMaterial(
		RTBEngine::ECS::MeshRenderer* renderer,
		RTBEngine::Rendering::Material* drawMaterial,
		OpaqueRenderBatchState& state)
	{
		if (!drawMaterial) {
			return;
		}

		const float alphaMultiplier = renderer ? renderer->GetOcclusionFadeAlpha() : 1.0f;

		RTBEngine::Rendering::Shader* nextShader = drawMaterial->GetShader();
		if (!nextShader) {
			return;
		}

		if (nextShader != state.shader) {
			nextShader->Bind();
			RTBEngine::Rendering::LightingUBO::GetInstance().Bind();
			RTBEngine::Rendering::CameraUBO::GetInstance().Bind();
			state.shader = nextShader;
			state.material = nullptr;
			state.texture = reinterpret_cast<RTBEngine::Rendering::Texture*>(static_cast<uintptr_t>(1));
			state.lastAlphaMultiplier = -1.0f;
		}

        if (drawMaterial != state.material || alphaMultiplier != state.lastAlphaMultiplier) {
            drawMaterial->ApplyProperties(alphaMultiplier);
            state.material = drawMaterial;
            state.lastAlphaMultiplier = alphaMultiplier;
        }

        if (renderer) {
            const std::string resolvedShaderName =
                renderer->shaderRef.empty() ? "basic" : renderer->shaderRef;
            RTBEngine::Rendering::ShaderProperties::ApplyExtraUniforms(
                nextShader,
                resolvedShaderName,
                renderer->shaderPropertyOverrides,
                renderer->colorRef);
            RTBEngine::Rendering::ShaderProperties::ApplyEngineUniforms(nextShader);
        }

        RTBEngine::Rendering::Texture* nextTexture = drawMaterial->GetTexture();
		if (nextTexture != state.texture) {
			if (nextTexture) {
				nextTexture->Bind(0);
			}
			state.texture = nextTexture;
		}
	}

	bool OpaqueMeshDrawLess(const OpaqueMeshDraw& a, const OpaqueMeshDraw& b)
	{
		const RTBEngine::Rendering::Material* aMaterial = a.material;
		const RTBEngine::Rendering::Material* bMaterial = b.material;
		const RTBEngine::Rendering::Shader* aShader = aMaterial ? aMaterial->GetShader() : nullptr;
		const RTBEngine::Rendering::Shader* bShader = bMaterial ? bMaterial->GetShader() : nullptr;
		const GLuint aProgram = aShader ? aShader->GetProgramID() : 0;
		const GLuint bProgram = bShader ? bShader->GetProgramID() : 0;

		if (aProgram != bProgram) {
			return aProgram < bProgram;
		}

		const RTBEngine::Rendering::Texture* aTexture = aMaterial ? aMaterial->GetTexture() : nullptr;
		const RTBEngine::Rendering::Texture* bTexture = bMaterial ? bMaterial->GetTexture() : nullptr;
		if (aTexture != bTexture) {
			return aTexture < bTexture;
		}

		if (aMaterial != bMaterial) {
			return aMaterial < bMaterial;
		}

		// Mesh before renderer so identical mesh+material draws are always adjacent (instancing).
		if (a.mesh != b.mesh) {
			return a.mesh < b.mesh;
		}

		return a.renderer < b.renderer;
	}

	bool RequiresOcclusionFadePass(RTBEngine::ECS::MeshRenderer* renderer)
	{
		return renderer && renderer->GetOcclusionFadeAlpha() < 0.999f;
	}

	void CollectMeshDraws(
		RTBEngine::ECS::MeshRenderer* renderer,
		std::vector<OpaqueMeshDraw>& opaqueDraws,
		std::vector<OpaqueMeshDraw>& transparentDraws)
	{
		if (!renderer) {
			return;
		}

		const auto collectDraw = [&](RTBEngine::Rendering::Mesh* drawMesh,
			RTBEngine::Rendering::Material* drawMaterial) {
			if (!drawMesh || !drawMaterial) {
				return;
			}

			const OpaqueMeshDraw draw{ renderer, drawMesh, drawMaterial };
			if (RequiresOcclusionFadePass(renderer)) {
				transparentDraws.push_back(draw);
			}
			else {
				opaqueDraws.push_back(draw);
			}
		};

		if (renderer->IsMultiMesh()) {
			const std::vector<RTBEngine::Rendering::Mesh*>& meshes = renderer->GetMeshes();
			for (int i = 0; i < renderer->GetMeshCount(); ++i) {
				RTBEngine::Rendering::Mesh* drawMesh = (i >= 0 && static_cast<std::size_t>(i) < meshes.size())
					? meshes[static_cast<std::size_t>(i)]
					: nullptr;
				collectDraw(drawMesh, renderer->GetMaterialForMesh(i));
			}
			return;
		}

		collectDraw(renderer->GetMesh(), renderer->GetMaterial());
	}

	void CollectOpaqueMeshDraws(
		RTBEngine::ECS::MeshRenderer* renderer,
		std::vector<OpaqueMeshDraw>& outDraws)
	{
		g_discardDrawScratch.clear();
		CollectMeshDraws(renderer, outDraws, g_discardDrawScratch);
	}

	float GetMeshDrawSortDistance(
		const OpaqueMeshDraw& draw,
		const RTBEngine::Math::Vector3& cameraPosition)
	{
		if (!draw.renderer || !draw.renderer->GetOwner()) {
			return 0.0f;
		}

		const RTBEngine::Math::Vector3 worldPosition = draw.renderer->GetOwner()->GetWorldPosition();
		return (worldPosition - cameraPosition).LengthSquared();
	}

	bool TransparentMeshDrawFartherFirst(
		const OpaqueMeshDraw& a,
		const OpaqueMeshDraw& b,
		const RTBEngine::Math::Vector3& cameraPosition)
	{
		return GetMeshDrawSortDistance(a, cameraPosition) > GetMeshDrawSortDistance(b, cameraPosition);
	}

	void RenderTransparentMeshDraws(
		const std::vector<OpaqueMeshDraw>& transparentDraws,
		const RTBEngine::Math::Vector3& cameraPosition)
	{
		if (transparentDraws.empty()) {
			return;
		}

		std::vector<OpaqueMeshDraw>& sortedDraws = g_sortedTransparentScratch;
		sortedDraws.assign(transparentDraws.begin(), transparentDraws.end());
		std::sort(
			sortedDraws.begin(),
			sortedDraws.end(),
			[&cameraPosition](const OpaqueMeshDraw& a, const OpaqueMeshDraw& b) {
				return TransparentMeshDrawFartherFirst(a, b, cameraPosition);
			});

		const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
		const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
		const GLboolean wasCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
		GLboolean wasDepthMaskEnabled = GL_TRUE;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMaskEnabled);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		OpaqueRenderBatchState batchState;
		for (const OpaqueMeshDraw& draw : sortedDraws) {
			ApplyOpaqueDrawMaterial(draw.renderer, draw.material, batchState);
			draw.renderer->RenderDraw(draw.mesh, draw.material);
		}

		if (wasCullFaceEnabled) {
			glEnable(GL_CULL_FACE);
		}
		else {
			glDisable(GL_CULL_FACE);
		}

		if (wasDepthTestEnabled) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}

		if (wasBlendEnabled) {
			glEnable(GL_BLEND);
		}
		else {
			glDisable(GL_BLEND);
		}

		glDepthMask(wasDepthMaskEnabled);
	}
}

RTBEngine::ECS::Scene::Scene(const std::string& name) : name(name)
{
}

RTBEngine::ECS::Scene::~Scene()
{
	while (!gameObjects.empty()) {
		GameObject* root = nullptr;

		for (const auto& gameObject : gameObjects) {
			GameObject* candidate = gameObject.get();
			if (!candidate) {
				continue;
			}

			GameObject* parent = candidate->GetParent();
			const bool parentInScene = parent && std::any_of(
				gameObjects.begin(),
				gameObjects.end(),
				[parent](const std::unique_ptr<GameObject>& obj) {
					return obj.get() == parent;
				});

			if (!parentInScene) {
				root = candidate;
				break;
			}
		}

		if (!root) {
			gameObjects.clear();
			break;
		}

		std::vector<GameObject*> hierarchy;
		CollectHierarchyPostOrder(root, hierarchy);

		for (GameObject* node : hierarchy) {
			if (node && node->GetParent()) {
				node->SetParent(nullptr);
			}
		}

		for (GameObject* node : hierarchy) {
			DestroyOwnedGameObject(gameObjects, node);
		}
	}

	pendingAdds.clear();
	gameObjectsByUuid.clear();
}

void RTBEngine::ECS::Scene::InvalidateComponentCaches()
{
	componentCachesDirty = true;
}

void RTBEngine::ECS::Scene::EnsureComponentCaches() const
{
	if (!componentCachesDirty) {
		return;
	}

	const_cast<Scene*>(this)->RebuildComponentCaches();
	componentCachesDirty = false;
}

void RTBEngine::ECS::Scene::RebuildComponentCaches() const
{
	cachedMeshRenderers.clear();
	cachedLightComponents.clear();
	cachedTrailRenderers.clear();
	cachedParticleSystems.clear();
	cachedCanvases.clear();
	cachedRigidBodies.clear();
	cachedOccludables.clear();
	cachedOcclusionTargets.clear();
	cachedComponentsByTypeName.clear();
	cachedComponentsByTypeId.clear();

	const auto collectFromGameObjects = [this](const std::vector<std::unique_ptr<GameObject>>& objects) {
		for (const auto& gameObject : objects) {
			if (!gameObject) {
				continue;
			}

			for (const GameObject::ComponentPtr& component : gameObject->GetComponents()) {
				if (!component) {
					continue;
				}

				const char* typeName = component->GetTypeName();
				if (typeName && typeName[0] != '\0') {
					cachedComponentsByTypeName[typeName].push_back(component.get());
					cachedComponentsByTypeId[TypeId::Hash(typeName)].push_back(component.get());
				}
			}

			if (MeshRenderer* meshRenderer = gameObject->GetComponent<MeshRenderer>()) {
				cachedMeshRenderers.push_back(meshRenderer);
			}

			if (LightComponent* lightComponent = gameObject->GetComponent<LightComponent>()) {
				cachedLightComponents.push_back(lightComponent);
			}

			if (TrailRenderer* trailRenderer = gameObject->GetComponent<TrailRenderer>()) {
				cachedTrailRenderers.push_back(trailRenderer);
			}

			if (ParticleSystem* particleSystem = gameObject->GetComponent<ParticleSystem>()) {
				cachedParticleSystems.push_back(particleSystem);
			}

			if (RTBEngine::UI::Canvas* canvas = gameObject->GetComponent<RTBEngine::UI::Canvas>()) {
				cachedCanvases.push_back(canvas);
			}

			if (RigidBodyComponent* rigidBody = gameObject->GetComponent<RigidBodyComponent>()) {
				cachedRigidBodies.push_back(rigidBody);
			}

			if (Occludable* occludable = gameObject->GetComponent<Occludable>()) {
				cachedOccludables.push_back(occludable);
			}

			if (OcclusionTarget* target = gameObject->GetComponent<OcclusionTarget>()) {
				cachedOcclusionTargets.push_back(target);
			}
		}
	};

	collectFromGameObjects(gameObjects);
	collectFromGameObjects(pendingAdds);
}

const std::vector<RTBEngine::ECS::MeshRenderer*>& RTBEngine::ECS::Scene::GetCachedMeshRenderers() const
{
	EnsureComponentCaches();
	return cachedMeshRenderers;
}

const std::vector<RTBEngine::ECS::LightComponent*>& RTBEngine::ECS::Scene::GetCachedLightComponents() const
{
	EnsureComponentCaches();
	return cachedLightComponents;
}

const std::vector<RTBEngine::ECS::TrailRenderer*>& RTBEngine::ECS::Scene::GetCachedTrailRenderers() const
{
	EnsureComponentCaches();
	return cachedTrailRenderers;
}

const std::vector<RTBEngine::ECS::ParticleSystem*>& RTBEngine::ECS::Scene::GetCachedParticleSystems() const
{
	EnsureComponentCaches();
	return cachedParticleSystems;
}

const std::vector<RTBEngine::UI::Canvas*>& RTBEngine::ECS::Scene::GetCachedCanvases() const
{
	EnsureComponentCaches();
	return cachedCanvases;
}

const std::vector<RTBEngine::ECS::RigidBodyComponent*>& RTBEngine::ECS::Scene::GetCachedRigidBodies() const
{
	EnsureComponentCaches();
	return cachedRigidBodies;
}

const std::vector<RTBEngine::ECS::Occludable*>& RTBEngine::ECS::Scene::GetCachedOccludables() const
{
	EnsureComponentCaches();
	return cachedOccludables;
}

const std::vector<RTBEngine::ECS::OcclusionTarget*>& RTBEngine::ECS::Scene::GetCachedOcclusionTargets() const
{
	EnsureComponentCaches();
	return cachedOcclusionTargets;
}

const std::vector<RTBEngine::ECS::Component*>& RTBEngine::ECS::Scene::GetCachedComponentsByTypeName(
	const char* typeName) const
{
	static const std::vector<Component*> kEmpty;

	EnsureComponentCaches();

	if (!typeName || typeName[0] == '\0') {
		return kEmpty;
	}

	const auto iterator = cachedComponentsByTypeName.find(typeName);
	if (iterator == cachedComponentsByTypeName.end()) {
		return kEmpty;
	}

	return iterator->second;
}

const std::vector<RTBEngine::ECS::Component*>& RTBEngine::ECS::Scene::GetCachedComponentsByTypeId(
	std::uint32_t typeId) const
{
	static const std::vector<Component*> kEmpty;

	EnsureComponentCaches();

	if (typeId == 0) {
		return kEmpty;
	}

	const auto iterator = cachedComponentsByTypeId.find(typeId);
	if (iterator == cachedComponentsByTypeId.end()) {
		return kEmpty;
	}

	return iterator->second;
}

void RTBEngine::ECS::Scene::RegisterGameObjectUuid(GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	const std::string& uuid = gameObject->GetUUID();
	if (uuid.empty()) {
		return;
	}

	const auto existing = gameObjectsByUuid.find(uuid);
	if (existing != gameObjectsByUuid.end() && existing->second != gameObject) {
		RTB_WARN("Scene: duplicate UUID '" + uuid + "'");
	}

	gameObjectsByUuid[uuid] = gameObject;
}

void RTBEngine::ECS::Scene::UnregisterGameObjectUuid(GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	const std::string& uuid = gameObject->GetUUID();
	if (uuid.empty()) {
		return;
	}

	const auto it = gameObjectsByUuid.find(uuid);
	if (it != gameObjectsByUuid.end() && it->second == gameObject) {
		gameObjectsByUuid.erase(it);
	}
}

void RTBEngine::ECS::Scene::RegisterGameObjectHierarchy(GameObject* root)
{
	if (!root) {
		return;
	}

	RegisterGameObjectUuid(root);

	for (GameObject* child : root->GetChildren()) {
		RegisterGameObjectHierarchy(child);
	}
}

void RTBEngine::ECS::Scene::UnregisterGameObjectHierarchy(GameObject* root)
{
	if (!root) {
		return;
	}

	for (GameObject* child : root->GetChildren()) {
		UnregisterGameObjectHierarchy(child);
	}

	UnregisterGameObjectUuid(root);
}

void RTBEngine::ECS::Scene::AssignGameObjectOwnership(GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	gameObject->SetOwningScene(this);
}

void RTBEngine::ECS::Scene::ClearGameObjectOwnership(GameObject* gameObject)
{
	if (!gameObject) {
		return;
	}

	gameObject->SetOwningScene(nullptr);
}

void RTBEngine::ECS::Scene::AddGameObject(GameObject* gameObject, bool queueLifecycle)
{
	if (iterationDepth > 0) {
		pendingAdds.push_back(std::unique_ptr<GameObject>(gameObject));
	} else {
		gameObjects.push_back(std::unique_ptr<GameObject>(gameObject));
	}

	if (queueLifecycle && lifecycleComplete && gameObject) {
		QueueLifecycleInitialization(gameObject);
	}

	AssignGameObjectOwnership(gameObject);
	RegisterGameObjectHierarchy(gameObject);
	InvalidateComponentCaches();
	pendingRenderLog = true;
}

void RTBEngine::ECS::Scene::BringGameObjectToLife(GameObject* root)
{
	if (!root) {
		return;
	}

	SceneLifecycle::BringHierarchyToLife(this, root);
}

void RTBEngine::ECS::Scene::QueueLifecycleInitialization(GameObject* root)
{
	if (!root) {
		return;
	}

	if (std::find(pendingLifecycleRoots.begin(), pendingLifecycleRoots.end(), root) == pendingLifecycleRoots.end()) {
		pendingLifecycleRoots.push_back(root);
	}
}

bool RTBEngine::ECS::Scene::OwnsGameObject(GameObject* target) const
{
	if (!target) {
		return false;
	}

	if (ContainsGameObject(gameObjects, target)) {
		return true;
	}

	return ContainsGameObject(pendingAdds, target);
}

void RTBEngine::ECS::Scene::FlushPendingLifecycle()
{
	if (!lifecycleComplete || pendingLifecycleRoots.empty()) {
		return;
	}

	std::vector<GameObject*> roots = std::move(pendingLifecycleRoots);
	pendingLifecycleRoots.clear();

	for (GameObject* root : roots) {
		if (!root || !OwnsGameObject(root)) {
			continue;
		}

		SceneLifecycle::BringHierarchyToLife(this, root);
	}
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

	PrunePendingLifecycleRoots(pendingLifecycleRoots, hierarchySet);

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
		UnregisterGameObjectUuid(node);
		ClearGameObjectOwnership(node);
		DestroyOwnedGameObject(gameObjects, node);
		DestroyOwnedGameObject(pendingAdds, node);
	}

	InvalidateComponentCaches();
}

void RTBEngine::ECS::Scene::FlushPendingCommands()
{
	if (iterationDepth > 0) return;

	const bool hadPendingChanges = !pendingRemoves.empty() || !pendingAdds.empty();

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
		AssignGameObjectOwnership(go.get());
		gameObjects.push_back(std::move(go));
	}
	pendingAdds.clear();

	if (hadPendingChanges) {
		InvalidateComponentCaches();
	}
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
	if (uuid.empty()) {
		return nullptr;
	}

	const auto it = gameObjectsByUuid.find(uuid);
	if (it == gameObjectsByUuid.end()) {
		return nullptr;
	}

	GameObject* gameObject = it->second;
	if (!gameObject || !OwnsGameObject(gameObject)) {
		return nullptr;
	}

	return gameObject;
}

void RTBEngine::ECS::Scene::PrepareForPlayMode()
{
	// Editor keeps the same scene between Play sessions; reset OnStart so RoundManager,
	// UI handlers, etc. run again when entering Play mode.
	std::function<void(GameObject*)> visitHierarchy = [&](GameObject* gameObject) {
		if (!gameObject) {
			return;
		}

		for (const auto& component : gameObject->GetComponents()) {
			if (component) {
				component->ResetStartInvocation();
			}
		}

		for (GameObject* child : gameObject->GetChildren()) {
			visitHierarchy(child);
		}
	};

	for (const auto& gameObject : gameObjects) {
		visitHierarchy(gameObject.get());
	}

	for (const auto& gameObject : pendingAdds) {
		visitHierarchy(gameObject.get());
	}
}

void RTBEngine::ECS::Scene::Update(float deltaTime)
{
	FlushPendingLifecycle();

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

	Navigation::ProcessSceneNavigationFixedUpdate(this);
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

	const Rendering::Frustum& frustum = camera->GetFrustum();
	std::vector<OpaqueMeshDraw>& opaqueDraws = g_opaqueDrawScratch;
	opaqueDraws.clear();
	opaqueDraws.reserve(GetCachedMeshRenderers().size());

	++iterationDepth;
	for (MeshRenderer* renderer : GetCachedMeshRenderers()) {
		if (!renderer || !renderer->IsEnabled()) {
			continue;
		}

		GameObject* gameObject = renderer->GetOwner();
		if (!gameObject || !gameObject->IsActiveInHierarchy()) {
			continue;
		}

		Math::Vector3 localMin, localMax;
		renderer->GetCombinedAABB(localMin, localMax);

		if (localMin != localMax) {
			Math::Vector3 worldMin, worldMax;
			Rendering::Frustum::TransformAABB(
				gameObject->GetWorldMatrix(), localMin, localMax, worldMin, worldMax);

			if (!frustum.IsAABBVisible(worldMin, worldMax)) {
				MeshRenderer::IncrementCulledCount();
				continue;
			}
		}

		CollectOpaqueMeshDraws(renderer, opaqueDraws);
	}

	std::sort(opaqueDraws.begin(), opaqueDraws.end(), OpaqueMeshDrawLess);

	OpaqueRenderBatchState batchState;
	std::vector<Math::Matrix4>& instanceMatrices = g_instanceMatrixScratch;

	const std::size_t drawCount = opaqueDraws.size();
	std::size_t i = 0;
	while (i < drawCount) {
		const OpaqueMeshDraw& first = opaqueDraws[i];

		// Extend the run while mesh and material stay identical (already adjacent after sorting).
		std::size_t j = i + 1;
		while (j < drawCount
			&& opaqueDraws[j].mesh == first.mesh
			&& opaqueDraws[j].material == first.material) {
			++j;
		}
		const std::size_t runLength = j - i;

		ApplyOpaqueDrawMaterial(first.renderer, first.material, batchState);

		// Instancing needs 2+ identical, non-skinned draws sharing mesh+material.
		bool canInstance = runLength >= 2 && first.mesh && first.material;
		if (canInstance) {
			for (std::size_t k = i; k < j; ++k) {
				if (DrawIsSkinned(opaqueDraws[k].renderer)) {
					canInstance = false;
					break;
				}
			}
		}

		if (canInstance) {
			instanceMatrices.clear();
			instanceMatrices.reserve(runLength);
			for (std::size_t k = i; k < j; ++k) {
				GameObject* owner = opaqueDraws[k].renderer ? opaqueDraws[k].renderer->GetOwner() : nullptr;
				if (owner) {
					instanceMatrices.push_back(owner->GetWorldMatrix());
				}
			}

			Rendering::Shader* shader = first.material->GetShader();
			if (shader && !instanceMatrices.empty()) {
				shader->SetBool("uUseInstancing", true);
				shader->SetBool("uHasAnimation", false);
				first.mesh->UploadInstanceData(instanceMatrices.data(), instanceMatrices.size());
				first.mesh->DrawInstanced(static_cast<GLsizei>(instanceMatrices.size()));
				shader->SetBool("uUseInstancing", false);
				MeshRenderer::AddInstancedDrawStats(
					first.mesh->GetIndexCount(),
					static_cast<uint32_t>(instanceMatrices.size()));
			}
		}
		else {
			for (std::size_t k = i; k < j; ++k) {
				opaqueDraws[k].renderer->RenderDraw(opaqueDraws[k].mesh, opaqueDraws[k].material);
			}
		}

		i = j;
	}

	--iterationDepth;
	FlushPendingCommands();
}

void RTBEngine::ECS::Scene::RenderTransparentEffects(Rendering::Camera* camera)
{
	if (!camera) return;

	++iterationDepth;

	const Rendering::Frustum& frustum = camera->GetFrustum();
	std::vector<OpaqueMeshDraw>& transparentDraws = g_transparentDrawScratch;
	transparentDraws.clear();

	for (MeshRenderer* renderer : GetCachedMeshRenderers()) {
		if (!renderer || !renderer->IsEnabled()) {
			continue;
		}

		GameObject* gameObject = renderer->GetOwner();
		if (!gameObject || !gameObject->IsActiveInHierarchy()) {
			continue;
		}

		Math::Vector3 localMin, localMax;
		renderer->GetCombinedAABB(localMin, localMax);

		if (localMin != localMax) {
			Math::Vector3 worldMin, worldMax;
			Rendering::Frustum::TransformAABB(
				gameObject->GetWorldMatrix(), localMin, localMax, worldMin, worldMax);

			if (!frustum.IsAABBVisible(worldMin, worldMax)) {
				continue;
			}
		}

		g_discardDrawScratch.clear();
		CollectMeshDraws(renderer, g_discardDrawScratch, transparentDraws);
	}

	RenderTransparentMeshDraws(transparentDraws, camera->GetPosition());

	for (TrailRenderer* trailRenderer : GetCachedTrailRenderers()) {
		if (!trailRenderer || !trailRenderer->IsEnabled()) {
			continue;
		}

		GameObject* gameObject = trailRenderer->GetOwner();
		if (!gameObject || !gameObject->IsActiveInHierarchy()) {
			continue;
		}

		trailRenderer->Render(camera);
	}

	for (ParticleSystem* particleSystem : GetCachedParticleSystems()) {
		if (!particleSystem || !particleSystem->IsEnabled()) {
			continue;
		}

		GameObject* gameObject = particleSystem->GetOwner();
		if (!gameObject || !gameObject->IsActiveInHierarchy()) {
			continue;
		}

		particleSystem->Render(camera);
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
		if (go && go->IsActiveInHierarchy()) count++;
	}
	return count;
}

uint32_t RTBEngine::ECS::Scene::GetActiveComponentCount() const {
	uint32_t count = 0;
	for (const auto& go : gameObjects) {
		if (go && go->IsActiveInHierarchy()) {
			count += static_cast<uint32_t>(go->GetComponents().size());
		}
	}
	return count;
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
