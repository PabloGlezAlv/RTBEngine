#include "DDGISystem.h"
#include "VulkanDDGIUpdater.h"
#include "OpenGLDDGIUpdater.h"
#include "DDGIUpdater.h"
#include "../Lighting/LightingProjectSettings.h"
#include "../RHI/RenderDevice.h"
#include "../RHI/GraphicsAPI.h"
#include "../../Scene/Scene.h"

namespace RTBEngine {
    namespace Rendering {
        namespace GI {

            DDGISystem& DDGISystem::GetInstance()
            {
                static DDGISystem instance;
                return instance;
            }

            DDGISystem::~DDGISystem()
            {
                Shutdown();
            }

            void DDGISystem::SyncFromProjectSettings()
            {
                const GI::DDGISettings settings = LightingProjectSettings::Get().GetDDGISettings();
                if (!projectVolume) {
                    projectVolume = std::make_unique<DDGIVolume>(settings);
                } else {
                    projectVolume->SetSettings(settings);
                }
            }

            void DDGISystem::Initialize()
            {
                if (initialized) return;
                if (!RHI::RenderDevice::HasDevice()) return;

                SyncFromProjectSettings();

                const auto api = RHI::RenderDevice::Get().GetAPI();
                if (api == RHI::GraphicsAPI::Vulkan) {
                    updater = std::make_unique<VulkanDDGIUpdater>();
                } else {
                    updater = std::make_unique<OpenGLDDGIUpdater>();
                }

                available = updater && updater->Initialize();
                initialized = true;
            }

            void DDGISystem::Shutdown()
            {
                if (!initialized) return;
                if (updater) {
                    updater->Shutdown();
                    updater.reset();
                }
                projectVolume.reset();
                available = false;
                initialized = false;
                frameIndex = 0;
            }

            void DDGISystem::Update(Scene::Scene* scene)
            {
                if (!initialized) Initialize();
                SyncFromProjectSettings();
                if (!available || !projectVolume || !scene) return;

                // Always keep GPU UBO (and ambient) ready, even when DDGI tracing is off.
                projectVolume->EnsureGpuResources(RHI::RenderDevice::Get());
                projectVolume->UploadUBO(RHI::RenderDevice::Get());

                if (!projectVolume->GetSettings().enabled) {
                    return;
                }

                updater->Update(*projectVolume, rtScene, scene, frameIndex++);
            }

            void DDGISystem::BindForShading()
            {
                if (!available || !projectVolume) return;
                // Bind even when DDGI is disabled so ambient fallback from UBO is available.
                if (!projectVolume->IsGpuReady()) {
                    projectVolume->EnsureGpuResources(RHI::RenderDevice::Get());
                    projectVolume->UploadUBO(RHI::RenderDevice::Get());
                }
                if (!projectVolume->IsGpuReady()) return;
                projectVolume->BindForSampling(RHI::RenderDevice::Get());
            }

        }
    }
}
