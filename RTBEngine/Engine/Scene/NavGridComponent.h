#pragma once



#include "../RTBEngineAPI.h"

#include "Component.h"

#include "../Navigation/NavGrid.h"

#include "../Navigation/NavGridBaker.h"
#include "../Navigation/NavMeshFile.h"

#include "../Reflection/PropertyMacros.h"

#include "../Math/Vectors/Vector3.h"

#include <string>
#include <vector>



namespace RTBEngine {

    namespace Physics {

        class PhysicsWorld;

    }



    namespace Scene {



        class Scene;



#pragma warning(push)

#pragma warning(disable: 4251)

        // Scene-level navigation surface. Editor bakes walkability; runtime registers the grid
        // with NavPathService so NavAgentComponents can pathfind.
        class RTB_API NavGridComponent : public Component {

        public:

            NavGridComponent();

            ~NavGridComponent() override;



            void OnAwake() override;

            void OnStart() override;

            void OnFixedUpdate(float fixedDeltaTime) override;

            void OnDestroy() override;

            void OnValidate() override;



            Math::Vector3 origin = Math::Vector3(-16.0f, 0.0f, -16.0f);

            Math::Vector3 size = Math::Vector3(32.0f, 0.0f, 32.0f);

            float cellSize = 0.5f;

            float agentRadius = 0.4f;

            float groundProbeHeight = 4.0f;



            const Navigation::NavGrid& GetGrid() const { return grid; }

            bool IsBaked() const { return baked; }

            int GetWalkableCellCount() const;



            Math::Vector3 GetWorldOrigin() const;

            Math::Vector3 GetWorldSize() const;



            bool BakeGrid();

            void ClearBakedGrid();

            void ActivateBakedGrid();



            std::string ExportWalkableHex() const;

            bool ImportWalkableHex(const std::string& hexData, int expectedWidth = 0, int expectedHeight = 0);



            void SetPendingWalkableImport(const std::string& hexData, int expectedWidth, int expectedHeight);

            void ApplyPendingWalkableImport();

            void RefreshBakedGridState();

            static void FinalizeImportsForScene(Scene* scene);

            static void ActivateAllBakedInScene(Scene* scene);

            static bool SceneHasNavGrid(Scene* scene);

            static void LoadNavMeshForScene(const std::string& sceneAssetPath, Scene* scene);
            static bool SaveNavMeshForScene(const std::string& sceneAssetPath, Scene* scene);

            static NavGridComponent* FindNavGridByOwnerUuid(Scene* scene, const std::string& ownerUuid);

            bool BuildNavMeshRecord(Navigation::NavMeshGridRecord& outRecord) const;
            bool ApplyNavMeshRecord(const Navigation::NavMeshGridRecord& record);

            RTB_COMPONENT(NavGridComponent)



        private:

            struct BakeFingerprint {

                Math::Vector3 localOrigin = Math::Vector3(0.0f, 0.0f, 0.0f);

                Math::Vector3 gridSize = Math::Vector3(0.0f, 0.0f, 0.0f);

                float gridCellSize = 0.0f;

                int gridWidth = 0;

                int gridHeight = 0;

            };

            // Detects stale .navmesh data when origin/size/cellSize change in the inspector.
            void ClampSettings();

            bool TryBakeGrid();

            void SyncBakedGridToWorld();

            void StoreBakeFingerprint();

            bool DoesFingerprintMatchCurrentSettings() const;

            bool RebuildGridFromCache();

            Physics::PhysicsWorld* ResolvePhysicsWorld() const;



            Navigation::NavGrid grid;

            Navigation::NavGridBaker baker;

            BakeFingerprint bakeFingerprint;

            bool baked = false;



            std::string pendingWalkableHex;

            int pendingGridWidth = 0;

            int pendingGridHeight = 0;

            std::string cachedWalkableHex;

            // -1 = invalid; recomputed after bake/import to avoid O(n) scans during activation.
            mutable int cachedWalkableCellCount = -1;
        };

#pragma warning(pop)



    }

}


