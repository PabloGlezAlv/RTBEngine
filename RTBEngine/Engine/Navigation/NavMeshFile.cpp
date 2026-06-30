#include "NavMeshFile.h"

#include "../Core/Logger.h"
#include "../Core/ResourceManager.h"
#include "../Scene/GameObject.h"
#include "../Scene/NavGridComponent.h"
#include "../Scene/Scene.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {
    // Binary sidecar format: Assets/Scenes/MyScene.navmesh next to MyScene.lua
    constexpr char kNavMeshMagic[8] = { 'R', 'T', 'B', 'N', 'A', 'V', 'M', '\0' };
    constexpr uint32_t kNavMeshVersion = 1;

    bool WriteU32(std::ostream& out, uint32_t value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return out.good();
    }

    bool ReadU32(std::istream& in, uint32_t& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good();
    }

    bool WriteF32(std::ostream& out, float value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return out.good();
    }

    bool ReadF32(std::istream& in, float& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good();
    }

    bool WriteI32(std::ostream& out, int32_t value)
    {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        return out.good();
    }

    bool ReadI32(std::istream& in, int32_t& value)
    {
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return in.good();
    }

    bool WriteString(std::ostream& out, const std::string& value)
    {
        const uint32_t length = static_cast<uint32_t>(value.size());
        if (!WriteU32(out, length)) {
            return false;
        }

        if (length == 0) {
            return true;
        }

        out.write(value.data(), static_cast<std::streamsize>(value.size()));
        return out.good();
    }

    bool ReadString(std::istream& in, std::string& value)
    {
        uint32_t length = 0;
        if (!ReadU32(in, length)) {
            return false;
        }

        if (length == 0) {
            value.clear();
            return true;
        }

        if (length > 1024u) {
            return false;
        }

        value.resize(length);
        in.read(value.data(), static_cast<std::streamsize>(length));
        return in.good();
    }

    void CollectNavMeshRecords(RTBEngine::ECS::Scene* scene, std::vector<RTBEngine::Navigation::NavMeshGridRecord>& outRecords)
    {
        outRecords.clear();
        if (!scene) {
            return;
        }

        std::function<void(RTBEngine::ECS::GameObject*)> visit = [&](RTBEngine::ECS::GameObject* gameObject) {
            if (!gameObject) {
                return;
            }

            if (auto* navGrid = gameObject->GetComponent<RTBEngine::ECS::NavGridComponent>()) {
                RTBEngine::Navigation::NavMeshGridRecord record;
                if (navGrid->BuildNavMeshRecord(record)) {
                    outRecords.push_back(std::move(record));
                }
            }

            for (RTBEngine::ECS::GameObject* child : gameObject->GetChildren()) {
                visit(child);
            }
        };

        for (const auto& gameObject : scene->GetGameObjects()) {
            if (gameObject) {
                visit(gameObject.get());
            }
        }
    }
}

namespace RTBEngine {
    namespace Navigation {

        std::string NavMeshFile::GetNavMeshPathForScene(const std::string& sceneAssetPath)
        {
            if (sceneAssetPath.empty()) {
                return {};
            }

            std::filesystem::path scenePath(sceneAssetPath);
            scenePath.replace_extension(".navmesh");
            return Core::ResourceManager::GetInstance().ResolvePathForRead(scenePath.generic_string());
        }

        bool NavMeshFile::SaveSceneNavMesh(const std::string& sceneAssetPath, ECS::Scene* scene)
        {
            if (sceneAssetPath.empty() || !scene) {
                return false;
            }

            std::vector<NavMeshGridRecord> records;
            CollectNavMeshRecords(scene, records);

            const std::string absolutePath = GetNavMeshPathForScene(sceneAssetPath);
            if (absolutePath.empty()) {
                return false;
            }

            // Never wipe an existing bake just because this particular call had nothing
            // to write (e.g. invoked during teardown). Deletion only happens explicitly.
            if (records.empty()) {
                return false;
            }

            const std::filesystem::path outputPath(absolutePath);
            std::error_code ec;
            const std::filesystem::path parentPath = outputPath.parent_path();
            if (!parentPath.empty()) {
                std::filesystem::create_directories(parentPath, ec);
                if (ec) {
                    RTB_WARN("[NavMeshFile] Failed to create directory '" + parentPath.string() +
                        "': " + ec.message());
                }
            }

            std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                RTB_WARN("[NavMeshFile] Failed to open for writing: " + absolutePath);
                return false;
            }

            file.write(kNavMeshMagic, sizeof(kNavMeshMagic));
            if (!WriteU32(file, kNavMeshVersion)) {
                return false;
            }

            const uint32_t gridCount = static_cast<uint32_t>(records.size());
            if (!WriteU32(file, gridCount)) {
                return false;
            }

            for (const NavMeshGridRecord& record : records) {
                if (!WriteString(file, record.ownerUuid)) {
                    return false;
                }

                if (!WriteF32(file, record.localOrigin.x) ||
                    !WriteF32(file, record.localOrigin.y) ||
                    !WriteF32(file, record.localOrigin.z) ||
                    !WriteF32(file, record.gridSize.x) ||
                    !WriteF32(file, record.gridSize.y) ||
                    !WriteF32(file, record.gridSize.z) ||
                    !WriteF32(file, record.cellSize) ||
                    !WriteI32(file, record.gridWidth) ||
                    !WriteI32(file, record.gridHeight)) {
                    return false;
                }

                const size_t expectedBytes = static_cast<size_t>(record.gridWidth * record.gridHeight);
                if (record.walkable.size() != expectedBytes) {
                    RTB_WARN("[NavMeshFile] Skipping invalid nav grid record for UUID " + record.ownerUuid);
                    return false;
                }

                file.write(reinterpret_cast<const char*>(record.walkable.data()),
                           static_cast<std::streamsize>(record.walkable.size()));
                if (!file.good()) {
                    return false;
                }
            }

            file.flush();
            const bool writeOk = file.good();
            file.close();

            if (!writeOk) {
                RTB_WARN("[NavMeshFile] Write stream failed for: " + absolutePath);
                return false;
            }

            return true;
        }

        bool NavMeshFile::LoadSceneNavMesh(const std::string& sceneAssetPath, ECS::Scene* scene)
        {
            if (sceneAssetPath.empty() || !scene) {
                return false;
            }

            const std::string absolutePath = GetNavMeshPathForScene(sceneAssetPath);
            if (absolutePath.empty()) {
                return false;
            }

            std::ifstream file(absolutePath, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }

            char magic[8] = {};
            file.read(magic, sizeof(magic));
            if (!file.good() || std::memcmp(magic, kNavMeshMagic, sizeof(kNavMeshMagic)) != 0) {
                RTB_WARN("[NavMeshFile] Invalid navmesh file header: " + absolutePath);
                return false;
            }

            uint32_t version = 0;
            uint32_t gridCount = 0;
            if (!ReadU32(file, version) || version != kNavMeshVersion || !ReadU32(file, gridCount)) {
                RTB_WARN("[NavMeshFile] Unsupported navmesh version in: " + absolutePath);
                return false;
            }

            int loadedGrids = 0;
            for (uint32_t gridIndex = 0; gridIndex < gridCount; ++gridIndex) {
                NavMeshGridRecord record;
                if (!ReadString(file, record.ownerUuid)) {
                    RTB_WARN("[NavMeshFile] Failed to read nav grid UUID from: " + absolutePath);
                    return false;
                }

                if (!ReadF32(file, record.localOrigin.x) ||
                    !ReadF32(file, record.localOrigin.y) ||
                    !ReadF32(file, record.localOrigin.z) ||
                    !ReadF32(file, record.gridSize.x) ||
                    !ReadF32(file, record.gridSize.y) ||
                    !ReadF32(file, record.gridSize.z) ||
                    !ReadF32(file, record.cellSize) ||
                    !ReadI32(file, record.gridWidth) ||
                    !ReadI32(file, record.gridHeight)) {
                    RTB_WARN("[NavMeshFile] Failed to read nav grid settings from: " + absolutePath);
                    return false;
                }

                if (record.gridWidth <= 0 || record.gridHeight <= 0) {
                    RTB_WARN("[NavMeshFile] Invalid nav grid dimensions in: " + absolutePath);
                    return false;
                }

                const size_t expectedBytes = static_cast<size_t>(record.gridWidth * record.gridHeight);
                record.walkable.resize(expectedBytes);
                file.read(reinterpret_cast<char*>(record.walkable.data()),
                          static_cast<std::streamsize>(expectedBytes));
                if (!file.good()) {
                    RTB_WARN("[NavMeshFile] Failed to read nav grid walkability from: " + absolutePath);
                    return false;
                }

                ECS::NavGridComponent* navGrid =
                    ECS::NavGridComponent::FindNavGridByOwnerUuid(scene, record.ownerUuid);
                if (!navGrid) {
                    continue;
                }

                if (navGrid->ApplyNavMeshRecord(record)) {
                    ++loadedGrids;
                } else {
                    RTB_WARN("[NavMeshFile] ApplyNavMeshRecord FAILED for UUID " + record.ownerUuid +
                        " (record " + std::to_string(record.gridWidth) + "x" + std::to_string(record.gridHeight) +
                        ", walkableBytes=" + std::to_string(record.walkable.size()) + ").");
                }
            }

            return loadedGrids > 0;
        }

        bool NavMeshFile::DeleteNavMeshFile(const std::string& sceneAssetPath)
        {
            const std::string absolutePath = GetNavMeshPathForScene(sceneAssetPath);
            if (absolutePath.empty()) {
                return false;
            }

            std::error_code ec;
            return std::filesystem::remove(absolutePath, ec);
        }

    }
}
