#include "NavGridComponent.h"

#include "../Navigation/NavPathService.h"
#include "../Physics/PhysicsWorld.h"
#include "GameObject.h"
#include "PhysicsWorldResolver.h"
#include "SceneManager.h"
#include "Scene.h"
#include "../Reflection/PropertyMacros.h"
#include "../Core/Logger.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>

namespace {
    int HexDigitValue(char character)
    {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }

        if (character >= 'a' && character <= 'f') {
            return 10 + (character - 'a');
        }

        if (character >= 'A' && character <= 'F') {
            return 10 + (character - 'A');
        }

        return -1;
    }

    void ActivateNavGridOnGameObject(RTBEngine::ECS::GameObject* gameObject)
    {
        if (!gameObject) {
            return;
        }

        if (auto* navGrid = gameObject->GetComponent<RTBEngine::ECS::NavGridComponent>()) {
            navGrid->RefreshBakedGridState();
        }

        for (RTBEngine::ECS::GameObject* child : gameObject->GetChildren()) {
            ActivateNavGridOnGameObject(child);
        }
    }
}

namespace RTBEngine {
    namespace ECS {

        using ThisClass = NavGridComponent;
        RTB_REGISTER_COMPONENT(NavGridComponent)
            RTB_PROPERTY(origin)
            RTB_PROPERTY(size)
            RTB_PROPERTY_RANGE(cellSize, 0.1f, 4.0f)
            RTB_PROPERTY_RANGE(agentRadius, 0.05f, 2.0f)
            RTB_PROPERTY_RANGE(groundProbeHeight, 0.5f, 20.0f)
        RTB_END_REGISTER(NavGridComponent)

        NavGridComponent::NavGridComponent() = default;

        NavGridComponent::~NavGridComponent()
        {
            if (Navigation::NavPathService::GetInstance().GetActiveGrid() == &grid) {
                Navigation::NavPathService::GetInstance().SetActiveGrid(nullptr);
            }
        }

        void NavGridComponent::OnAwake()
        {
            ClampSettings();
            RefreshBakedGridState();
        }

        void NavGridComponent::OnStart()
        {
            RefreshBakedGridState();
            ActivateBakedGrid();
        }

        void NavGridComponent::OnFixedUpdate(float /*fixedDeltaTime*/)
        {
        }

        void NavGridComponent::OnDestroy()
        {
            if (Navigation::NavPathService::GetInstance().GetActiveGrid() == &grid) {
                Navigation::NavPathService::GetInstance().SetActiveGrid(nullptr);
            }
        }

        void NavGridComponent::OnValidate()
        {
            ClampSettings();
            RefreshBakedGridState();
        }

        int NavGridComponent::GetWalkableCellCount() const
        {
            if (cachedWalkableCellCount >= 0) {
                return cachedWalkableCellCount;
            }

            if (!grid.IsConfigured()) {
                cachedWalkableCellCount = 0;
                return 0;
            }

            int count = 0;
            for (uint8_t walkable : grid.GetWalkableData()) {
                if (walkable != 0) {
                    ++count;
                }
            }

            cachedWalkableCellCount = count;
            return count;
        }

        Math::Vector3 NavGridComponent::GetWorldOrigin() const
        {
            // origin is local to the owner; pathfinding always uses world-space cell bounds.
            Math::Vector3 worldOrigin = origin;
            if (owner) {
                const Math::Vector3 ownerPosition = owner->GetWorldPosition();
                worldOrigin.x += ownerPosition.x;
                worldOrigin.y += ownerPosition.y;
                worldOrigin.z += ownerPosition.z;
            }
            return worldOrigin;
        }

        Math::Vector3 NavGridComponent::GetWorldSize() const
        {
            return size;
        }

        bool NavGridComponent::BakeGrid()
        {
            return TryBakeGrid();
        }

        void NavGridComponent::ClearBakedGrid()
        {
            baked = false;
            grid.ClearWalkability();
            bakeFingerprint = {};
            cachedWalkableHex.clear();
            cachedWalkableCellCount = -1;

            if (Navigation::NavPathService::GetInstance().GetActiveGrid() == &grid) {
                Navigation::NavPathService::GetInstance().SetActiveGrid(nullptr);
            }

            if (SceneManager::GetInstance().GetActiveScene()) {
                Navigation::NavMeshFile::DeleteNavMeshFile(
                    SceneManager::GetInstance().GetActiveScenePath());
            }
        }

        bool NavGridComponent::RebuildGridFromCache()
        {
            if (cachedWalkableHex.empty() ||
                bakeFingerprint.gridWidth <= 0 ||
                bakeFingerprint.gridHeight <= 0) {
                return false;
            }

            return ImportWalkableHex(cachedWalkableHex,
                                     bakeFingerprint.gridWidth,
                                     bakeFingerprint.gridHeight);
        }

        void NavGridComponent::ActivateBakedGrid()
        {
            if (!baked && !cachedWalkableHex.empty()) {
                baked = true;
            }

            if (!baked) {
                return;
            }

            if (!DoesFingerprintMatchCurrentSettings()) {
                RTB_WARN("[NavGridComponent] Baked navigation data is out of date with current grid settings.");
                return;
            }

            if (!grid.IsConfigured() || GetWalkableCellCount() <= 0) {
                if (!RebuildGridFromCache()) {
                    RTB_WARN("[NavGridComponent] Cannot activate navigation grid: no walkable cells.");
                    return;
                }
            }

            // Publish this component's grid as the scene-wide navigation field.
            auto& pathService = Navigation::NavPathService::GetInstance();
            if (pathService.GetActiveGrid() != &grid) {
                pathService.SetActiveGrid(&grid);
            }
        }

        std::string NavGridComponent::ExportWalkableHex() const
        {
            if (!baked || !grid.IsConfigured()) {
                return {};
            }

            static constexpr char kHexDigits[] = "0123456789ABCDEF";
            const std::vector<uint8_t>& walkable = grid.GetWalkableData();

            std::string hex;
            hex.reserve(walkable.size() * 2);
            for (uint8_t value : walkable) {
                hex.push_back(kHexDigits[(value >> 4) & 0x0F]);
                hex.push_back(kHexDigits[value & 0x0F]);
            }

            return hex;
        }

        void NavGridComponent::SetPendingWalkableImport(const std::string& hexData,
                                                          int expectedWidth,
                                                          int expectedHeight)
        {
            pendingWalkableHex = hexData;
            pendingGridWidth = expectedWidth;
            pendingGridHeight = expectedHeight;
        }

        void NavGridComponent::ApplyPendingWalkableImport()
        {
            if (pendingWalkableHex.empty()) {
                return;
            }

            const std::string hexData = std::move(pendingWalkableHex);
            const int expectedWidth = pendingGridWidth;
            const int expectedHeight = pendingGridHeight;
            pendingWalkableHex.clear();
            pendingGridWidth = 0;
            pendingGridHeight = 0;

            ImportWalkableHex(hexData, expectedWidth, expectedHeight);
        }

        void NavGridComponent::FinalizeImportsForScene(Scene* scene)
        {
            if (!scene) {
                return;
            }

            for (const auto& gameObject : scene->GetGameObjects()) {
                if (!gameObject) {
                    continue;
                }

                std::function<void(GameObject*)> visit = [&](GameObject* node) {
                    if (!node) {
                        return;
                    }

                    if (auto* navGrid = node->GetComponent<NavGridComponent>()) {
                        navGrid->ApplyPendingWalkableImport();
                    }

                    for (GameObject* child : node->GetChildren()) {
                        visit(child);
                    }
                };

                visit(gameObject.get());
            }
        }

        bool NavGridComponent::ImportWalkableHex(const std::string& hexData,
                                                 int expectedWidth,
                                                 int expectedHeight)
        {
            if (hexData.empty()) {
                return false;
            }

            cachedWalkableCellCount = -1;

            int width = expectedWidth;
            int height = expectedHeight;
            if (width <= 0 || height <= 0) {
                width = std::max(1, static_cast<int>(std::ceil(std::max(size.x, 0.1f) / cellSize)));
                height = std::max(1, static_cast<int>(std::ceil(std::max(size.z, 0.1f) / cellSize)));
            }

            const size_t expectedBytes = static_cast<size_t>(width * height);
            if (hexData.size() != expectedBytes * 2) {
                RTB_WARN("[NavGridComponent] Invalid walkableData size (hex=" +
                    std::to_string(hexData.size()) + ", expected=" + std::to_string(expectedBytes * 2) +
                    ", grid=" + std::to_string(width) + "x" + std::to_string(height) + ").");
                baked = false;
                return false;
            }

            std::vector<uint8_t> imported;
            imported.reserve(expectedBytes);

            for (size_t index = 0; index < hexData.size(); index += 2) {
                const int high = HexDigitValue(hexData[index]);
                const int low = HexDigitValue(hexData[index + 1]);
                if (high < 0 || low < 0) {
                    RTB_WARN("[NavGridComponent] Invalid walkableData hex encoding.");
                    baked = false;
                    return false;
                }

                imported.push_back(static_cast<uint8_t>((high << 4) | low));
            }

            grid.Configure(GetWorldOrigin(), size, cellSize);
            if (grid.GetWidth() != width || grid.GetHeight() != height) {
                RTB_WARN("[NavGridComponent] Imported walkableData dimensions (" +
                    std::to_string(width) + "x" + std::to_string(height) +
                    ") do not match configured grid (" +
                    std::to_string(grid.GetWidth()) + "x" + std::to_string(grid.GetHeight()) + ").");
                baked = false;
                return false;
            }

            if (!grid.SetWalkabilityData(imported)) {
                baked = false;
                return false;
            }

            if (GetWalkableCellCount() <= 0) {
                RTB_WARN("[NavGridComponent] Imported navigation data has zero walkable cells.");
                baked = false;
                grid.ClearWalkability();
                return false;
            }

            baked = true;
            cachedWalkableHex = ExportWalkableHex();
            cachedWalkableCellCount = GetWalkableCellCount();
            StoreBakeFingerprint();
            ActivateBakedGrid();
            RTB_INFO("[NavGridComponent] Imported navigation grid (" +
                std::to_string(width) + "x" + std::to_string(height) +
                ", walkable=" + std::to_string(GetWalkableCellCount()) + ").");
            return true;
        }

        void NavGridComponent::ActivateAllBakedInScene(Scene* scene)
        {
            if (!scene) {
                return;
            }

            for (const auto& gameObject : scene->GetGameObjects()) {
                if (!gameObject) {
                    continue;
                }

                ActivateNavGridOnGameObject(gameObject.get());
            }
        }

        void NavGridComponent::SyncBakedGridToWorld()
        {
            if (!baked) {
                return;
            }

            if (!grid.IsConfigured()) {
                RebuildGridFromCache();
                return;
            }

            const std::vector<uint8_t> walkableData = grid.GetWalkableData();
            const int previousWidth = grid.GetWidth();
            const int previousHeight = grid.GetHeight();

            grid.Configure(GetWorldOrigin(), size, cellSize);

            if (grid.GetWidth() != previousWidth || grid.GetHeight() != previousHeight) {
                RTB_WARN("[NavGridComponent] Grid dimensions changed while syncing world transform; rebuilding from cache.");
                RebuildGridFromCache();
                return;
            }

            if (!grid.SetWalkabilityData(walkableData)) {
                RebuildGridFromCache();
            }

            cachedWalkableCellCount = -1;
        }

        void NavGridComponent::RefreshBakedGridState()
        {
            if (!baked && !cachedWalkableHex.empty()) {
                baked = true;
            }

            if (!baked) {
                return;
            }

            if (!DoesFingerprintMatchCurrentSettings()) {
                RTB_WARN("[NavGridComponent] Baked navigation data no longer matches grid settings.");
                return;
            }

            if (!grid.IsConfigured() || GetWalkableCellCount() <= 0) {
                RebuildGridFromCache();
            } else {
                SyncBakedGridToWorld();
            }

            ActivateBakedGrid();
        }

        void NavGridComponent::ClampSettings()
        {
            cellSize = std::max(cellSize, 0.1f);
            agentRadius = std::max(agentRadius, 0.05f);
            groundProbeHeight = std::max(groundProbeHeight, 0.5f);
            size.x = std::max(size.x, cellSize);
            size.z = std::max(size.z, cellSize);
        }

        bool NavGridComponent::TryBakeGrid()
        {
            Physics::PhysicsWorld* physicsWorld = ResolvePhysicsWorld();
            if (!physicsWorld) {
                RTB_WARN("[NavGridComponent] Cannot bake navigation grid: physics world is not ready.");
                return false;
            }

            const Math::Vector3 worldOrigin = GetWorldOrigin();
            grid.Configure(worldOrigin, size, cellSize);
            cachedWalkableCellCount = -1;

            Navigation::NavGridBakeSettings settings;
            settings.agentRadius = agentRadius;
            settings.groundProbeHeight = groundProbeHeight;
            settings.groundProbeDepth = std::max(groundProbeHeight + 2.0f, 6.0f);

            int walkableCells = 0;
            if (!baker.Bake(grid, *physicsWorld, settings, &walkableCells)) {
                baked = false;
                RTB_WARN("[NavGridComponent] Failed to bake navigation grid.");
                return false;
            }

            if (walkableCells <= 0) {
                baked = false;
                RTB_WARN("[NavGridComponent] Baked navigation grid has zero walkable cells. Check origin/size and colliders.");
                return false;
            }

            baked = true;
            cachedWalkableHex = ExportWalkableHex();
            cachedWalkableCellCount = walkableCells;
            StoreBakeFingerprint();
            ActivateBakedGrid();
            if (Scene* scene = SceneManager::GetInstance().GetActiveScene()) {
                SaveNavMeshForScene(SceneManager::GetInstance().GetActiveScenePath(), scene);
            }
            RTB_INFO("[NavGridComponent] Baked navigation grid (" +
                std::to_string(grid.GetWidth()) + "x" + std::to_string(grid.GetHeight()) +
                ", walkable=" + std::to_string(walkableCells) + ").");
            return true;
        }

        void NavGridComponent::StoreBakeFingerprint()
        {
            bakeFingerprint.localOrigin = origin;
            bakeFingerprint.gridSize = size;
            bakeFingerprint.gridCellSize = cellSize;
            bakeFingerprint.gridWidth = grid.GetWidth();
            bakeFingerprint.gridHeight = grid.GetHeight();
        }

        bool NavGridComponent::DoesFingerprintMatchCurrentSettings() const
        {
            if (bakeFingerprint.gridCellSize <= 0.0f ||
                bakeFingerprint.gridWidth <= 0 ||
                bakeFingerprint.gridHeight <= 0) {
                return false;
            }

            constexpr float kEpsilon = 0.001f;

            const int expectedWidth =
                std::max(1, static_cast<int>(std::ceil(std::max(size.x, 0.1f) / cellSize)));
            const int expectedHeight =
                std::max(1, static_cast<int>(std::ceil(std::max(size.z, 0.1f) / cellSize)));

            return std::abs(bakeFingerprint.localOrigin.x - origin.x) < kEpsilon &&
                   std::abs(bakeFingerprint.localOrigin.y - origin.y) < kEpsilon &&
                   std::abs(bakeFingerprint.localOrigin.z - origin.z) < kEpsilon &&
                   std::abs(bakeFingerprint.gridSize.x - size.x) < kEpsilon &&
                   std::abs(bakeFingerprint.gridSize.z - size.z) < kEpsilon &&
                   std::abs(bakeFingerprint.gridCellSize - cellSize) < kEpsilon &&
                   bakeFingerprint.gridWidth == expectedWidth &&
                   bakeFingerprint.gridHeight == expectedHeight;
        }

        bool NavGridComponent::BuildNavMeshRecord(Navigation::NavMeshGridRecord& outRecord) const
        {
            if (!baked || !grid.IsConfigured() || GetWalkableCellCount() <= 0 || !owner) {
                return false;
            }

            outRecord.ownerUuid = owner->GetUUID();
            outRecord.localOrigin = origin;
            outRecord.gridSize = size;
            outRecord.cellSize = cellSize;
            outRecord.gridWidth = grid.GetWidth();
            outRecord.gridHeight = grid.GetHeight();
            outRecord.walkable = grid.GetWalkableData();
            return !outRecord.ownerUuid.empty() && !outRecord.walkable.empty();
        }

        bool NavGridComponent::ApplyNavMeshRecord(const Navigation::NavMeshGridRecord& record)
        {
            if (record.walkable.empty() || record.gridWidth <= 0 || record.gridHeight <= 0) {
                RTB_WARN("[NavGridComponent] ApplyNavMeshRecord: empty/invalid record.");
                return false;
            }

            origin = record.localOrigin;
            size = record.gridSize;
            cellSize = record.cellSize;
            ClampSettings();

            grid.Configure(GetWorldOrigin(), size, cellSize);
            if (grid.GetWidth() != record.gridWidth || grid.GetHeight() != record.gridHeight) {
                RTB_WARN("[NavGridComponent] Navmesh dimensions mismatch: record=" +
                    std::to_string(record.gridWidth) + "x" + std::to_string(record.gridHeight) +
                    " configured=" + std::to_string(grid.GetWidth()) + "x" + std::to_string(grid.GetHeight()) +
                    " (size=" + std::to_string(size.x) + "," + std::to_string(size.z) +
                    " cellSize=" + std::to_string(cellSize) + ").");
                return false;
            }

            if (!grid.SetWalkabilityData(record.walkable)) {
                RTB_WARN("[NavGridComponent] ApplyNavMeshRecord: SetWalkabilityData failed (data=" +
                    std::to_string(record.walkable.size()) + ", grid=" +
                    std::to_string(grid.GetWidth() * grid.GetHeight()) + ").");
                return false;
            }

            if (GetWalkableCellCount() <= 0) {
                RTB_WARN("[NavGridComponent] ApplyNavMeshRecord: zero walkable cells after apply.");
                return false;
            }

            baked = true;
            cachedWalkableHex = ExportWalkableHex();
            cachedWalkableCellCount = GetWalkableCellCount();
            StoreBakeFingerprint();
            ActivateBakedGrid();
            return true;
        }

        NavGridComponent* NavGridComponent::FindNavGridByOwnerUuid(Scene* scene, const std::string& ownerUuid)
        {
            if (!scene || ownerUuid.empty()) {
                return nullptr;
            }

            GameObject* gameObject = scene->FindGameObjectByUUID(ownerUuid);
            if (!gameObject) {
                return nullptr;
            }

            return gameObject->GetComponent<NavGridComponent>();
        }

        bool NavGridComponent::SceneHasNavGrid(Scene* scene)
        {
            if (!scene) {
                return false;
            }

            std::function<bool(GameObject*)> visit = [&](GameObject* gameObject) -> bool {
                if (!gameObject) {
                    return false;
                }

                if (gameObject->GetComponent<NavGridComponent>()) {
                    return true;
                }

                for (GameObject* child : gameObject->GetChildren()) {
                    if (visit(child)) {
                        return true;
                    }
                }

                return false;
            };

            for (const auto& gameObject : scene->GetGameObjects()) {
                if (gameObject && visit(gameObject.get())) {
                    return true;
                }
            }

            return false;
        }

        void NavGridComponent::LoadNavMeshForScene(const std::string& sceneAssetPath, Scene* scene)
        {
            if (!SceneHasNavGrid(scene)) {
                return;
            }

            Navigation::NavMeshFile::LoadSceneNavMesh(sceneAssetPath, scene);
        }

        bool NavGridComponent::SaveNavMeshForScene(const std::string& sceneAssetPath, Scene* scene)
        {
            if (!SceneHasNavGrid(scene)) {
                return false;
            }

            return Navigation::NavMeshFile::SaveSceneNavMesh(sceneAssetPath, scene);
        }

        Physics::PhysicsWorld* NavGridComponent::ResolvePhysicsWorld() const
        {
            if (Physics::PhysicsWorld* world = ResolvePhysicsWorldFromGameObject(owner)) {
                return world;
            }

            if (Scene* scene = SceneManager::GetInstance().GetActiveScene()) {
                return ResolvePhysicsWorldFromScene(scene);
            }

            return nullptr;
        }

    }
}
