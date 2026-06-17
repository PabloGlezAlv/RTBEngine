#pragma once

#include "../RTBEngineAPI.h"
#include "../Math/Vectors/Vector3.h"
#include <cstdint>
#include <vector>

namespace RTBEngine {
    namespace Navigation {

        // 2D walkability grid in world XZ. Baked by NavGridBaker, persisted in NavMeshFile (.navmesh).
        class RTB_API NavGrid {
        public:
            void Configure(const Math::Vector3& gridOrigin,
                           const Math::Vector3& gridSize,
                           float gridCellSize);

            bool IsConfigured() const { return width > 0 && height > 0 && cellSize > 0.0f; }
            int GetWidth() const { return width; }
            int GetHeight() const { return height; }
            float GetCellSize() const { return cellSize; }
            const Math::Vector3& GetOrigin() const { return origin; }
            const Math::Vector3& GetSize() const { return size; }

            void ClearWalkability();
            void SetWalkable(int cellX, int cellZ, bool walkable);
            bool IsWalkable(int cellX, int cellZ) const;
            bool IsWalkableIndex(int index) const;

            bool WorldToCell(const Math::Vector3& worldPosition, int& outCellX, int& outCellZ) const;
            // Projects out-of-bounds world positions onto the nearest border cell.
            bool WorldToCellClamped(const Math::Vector3& worldPosition, int& outCellX, int& outCellZ) const;
            bool CellToWorld(int cellX, int cellZ, Math::Vector3& outWorld) const;
            int CellToIndex(int cellX, int cellZ) const;
            void IndexToCell(int index, int& outCellX, int& outCellZ) const;

            bool ClampToNearestWalkable(int& inOutCellX, int& inOutCellZ, int maxRadius) const;

            const std::vector<uint8_t>& GetWalkableData() const { return walkable; }
            bool SetWalkabilityData(const std::vector<uint8_t>& data);

        private:
            bool IsInside(int cellX, int cellZ) const;

            Math::Vector3 origin = Math::Vector3(0.0f, 0.0f, 0.0f);
            Math::Vector3 size = Math::Vector3(32.0f, 0.0f, 32.0f);
            float cellSize = 0.5f;
            int width = 0;
            int height = 0;
            std::vector<uint8_t> walkable;
        };

    }
}
