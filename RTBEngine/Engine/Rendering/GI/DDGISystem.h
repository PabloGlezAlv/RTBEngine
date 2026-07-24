#pragma once

#include "../../RTBEngineAPI.h"
#include "DDGIVolume.h"
#include "RayTracingScene.h"
#include "DDGIUpdater.h"
#include <memory>

namespace RTBEngine {
    namespace Scene {
        class Scene;
    }

    namespace Rendering {
        namespace GI {

#pragma warning(push)
#pragma warning(disable: 4251)
            class RTB_API DDGISystem {
            public:
                static DDGISystem& GetInstance();

                void Initialize();
                void Shutdown();

                void SyncFromProjectSettings();

                DDGIVolume* GetActiveVolume() const { return projectVolume.get(); }

                void Update(Scene::Scene* scene);
                void BindForShading();

                bool IsAvailable() const { return available; }

            private:
                DDGISystem() = default;
                ~DDGISystem();

                std::unique_ptr<DDGIVolume> projectVolume;
                std::unique_ptr<DDGIUpdater> updater;
                RayTracingScene rtScene;
                bool available = false;
                bool initialized = false;
                int frameIndex = 0;
            };
#pragma warning(pop)

        }
    }
}
