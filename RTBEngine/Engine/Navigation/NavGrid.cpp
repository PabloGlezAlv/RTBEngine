#include "NavGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace RTBEngine {
    namespace Navigation {

        void NavGrid::Configure(const Math::Vector3& gridOrigin,
                                const Math::Vector3& gridSize,
                                float gridCellSize)
        {
            origin = gridOrigin;
            size = gridSize;
            cellSize = std::max(gridCellSize, 0.1f);

            width = std::max(1, static_cast<int>(std::ceil(std::max(size.x, 0.1f) / cellSize)));
            height = std::max(1, static_cast<int>(std::ceil(std::max(size.z, 0.1f) / cellSize)));
            walkable.assign(static_cast<size_t>(width * height), 0);
        }

        void NavGrid::ClearWalkability()
        {
            std::fill(walkable.begin(), walkable.end(), static_cast<uint8_t>(0));
        }

        void NavGrid::SetWalkable(int cellX, int cellZ, bool isWalkable)
        {
            if (!IsInside(cellX, cellZ)) {
                return;
            }

            walkable[static_cast<size_t>(CellToIndex(cellX, cellZ))] = isWalkable ? 1u : 0u;
        }

        bool NavGrid::IsWalkable(int cellX, int cellZ) const
        {
            if (!IsInside(cellX, cellZ)) {
                return false;
            }

            return walkable[static_cast<size_t>(CellToIndex(cellX, cellZ))] != 0;
        }

        bool NavGrid::IsWalkableIndex(int index) const
        {
            if (index < 0 || index >= static_cast<int>(walkable.size())) {
                return false;
            }

            return walkable[static_cast<size_t>(index)] != 0;
        }

        bool NavGrid::WorldToCell(const Math::Vector3& worldPosition, int& outCellX, int& outCellZ) const
        {
            if (!IsConfigured()) {
                return false;
            }

            const float localX = worldPosition.x - origin.x;
            const float localZ = worldPosition.z - origin.z;
            if (localX < 0.0f || localZ < 0.0f || localX > size.x || localZ > size.z) {
                return false;
            }

            outCellX = std::clamp(static_cast<int>(localX / cellSize), 0, width - 1);
            outCellZ = std::clamp(static_cast<int>(localZ / cellSize), 0, height - 1);
            return true;
        }

        bool NavGrid::CellToWorld(int cellX, int cellZ, Math::Vector3& outWorld) const
        {
            if (!IsInside(cellX, cellZ)) {
                return false;
            }

            outWorld.x = origin.x + (static_cast<float>(cellX) + 0.5f) * cellSize;
            outWorld.y = origin.y;
            outWorld.z = origin.z + (static_cast<float>(cellZ) + 0.5f) * cellSize;
            return true;
        }

        int NavGrid::CellToIndex(int cellX, int cellZ) const
        {
            return cellZ * width + cellX;
        }

        void NavGrid::IndexToCell(int index, int& outCellX, int& outCellZ) const
        {
            outCellX = index % width;
            outCellZ = index / width;
        }

        bool NavGrid::ClampToNearestWalkable(int& inOutCellX, int& inOutCellZ, int maxRadius) const
        {
            if (IsWalkable(inOutCellX, inOutCellZ)) {
                return true;
            }

            for (int radius = 1; radius <= maxRadius; ++radius) {
                for (int dz = -radius; dz <= radius; ++dz) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dz)) != radius) {
                            continue;
                        }

                        const int candidateX = inOutCellX + dx;
                        const int candidateZ = inOutCellZ + dz;
                        if (IsWalkable(candidateX, candidateZ)) {
                            inOutCellX = candidateX;
                            inOutCellZ = candidateZ;
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        bool NavGrid::IsInside(int cellX, int cellZ) const
        {
            return cellX >= 0 && cellZ >= 0 && cellX < width && cellZ < height;
        }

        bool NavGrid::SetWalkabilityData(const std::vector<uint8_t>& data)
        {
            if (!IsConfigured() || data.size() != walkable.size()) {
                return false;
            }

            walkable = data;
            return true;
        }

    }
}
