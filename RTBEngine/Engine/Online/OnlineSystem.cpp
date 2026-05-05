#include "OnlineSystem.h"

#include "EosOnlineBackend.h"
#include "IOnlineBackend.h"
#include "NullOnlineBackend.h"
#include "OnlineConfig.h"
#include "../Core/Logger.h"

#include <memory>
#include <string>

namespace RTBEngine {
    namespace Online {

        OnlineSystem& OnlineSystem::GetInstance()
        {
            static OnlineSystem instance;
            return instance;
        }

        OnlineSystem::~OnlineSystem()
        {
            Shutdown();
        }

        // Starts the selected backend while keeping online optional by default.
        bool OnlineSystem::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            enabled = config.enabled;
            failApplicationOnError = config.failApplicationOnError;
            backendType = config.backend;
            lastError.clear();

            if (!enabled) {
                state = OnlineState::Disabled;
                RTB_INFO("OnlineSystem: disabled.");
                return true;
            }

            // Resolve the backend implementation from configuration.
            backend = CreateBackend(backendType);
            if (!backend) {
                state = OnlineState::Error;
                lastError = std::string("Unsupported online backend: ") + ToString(backendType);
                RTB_ERROR("OnlineSystem: " + lastError);
                return !failApplicationOnError;
            }

            // Backend failures can either stop the application or degrade gracefully.
            if (!backend->Initialize(config)) {
                state = OnlineState::Error;
                const char* backendError = backend->GetLastError();
                lastError = backendError && backendError[0] != '\0'
                    ? backendError
                    : "Online backend initialization failed.";

                RTB_ERROR("OnlineSystem: " + lastError);
                backend.reset();
                return !failApplicationOnError;
            }

            state = OnlineState::Initialized;
            RTB_INFO(std::string("OnlineSystem: initialized with backend ") + backend->GetName() + ".");
            return true;
        }

        void OnlineSystem::Tick(float deltaTime)
        {
            if (state != OnlineState::Initialized || !backend) {
                return;
            }

            backend->Tick(deltaTime);
        }

        void OnlineSystem::Shutdown()
        {
            if (backend) {
                backend->Shutdown();
                backend.reset();
            }

            enabled = false;
            failApplicationOnError = false;
            state = OnlineState::Disabled;
        }

        IOnlineIdentity* OnlineSystem::GetIdentity()
        {
            return backend ? backend->GetIdentity() : nullptr;
        }

        const IOnlineIdentity* OnlineSystem::GetIdentity() const
        {
            return backend ? backend->GetIdentity() : nullptr;
        }

        // Centralized backend factory
        std::unique_ptr<IOnlineBackend> OnlineSystem::CreateBackend(OnlineBackendType type)
        {
            switch (type) {
            case OnlineBackendType::Null:
                return std::make_unique<NullOnlineBackend>();
            case OnlineBackendType::EOS:
                return std::make_unique<EosOnlineBackend>();
            default:
                return nullptr;
            }
        }

    }
}
