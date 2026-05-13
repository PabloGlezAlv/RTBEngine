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
            // Reset any previous backend before applying the new configuration.
            Shutdown();

            // Cache the requested runtime configuration.
            enabled = config.enabled;
            failApplicationOnError = config.failApplicationOnError;
            backendType = config.backend;
            defaultLoginOptions.type = config.loginType;
            defaultLoginOptions.displayName = config.loginDisplayName;
            defaultLoginOptions.developerAuthHost = config.developerAuthHost;
            defaultLoginOptions.developerAuthCredentialName = config.developerAuthCredentialName;
            lastError.clear();

            // Disabled online is a valid no-op startup mode.
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

            // Let the selected backend perform SDK-specific initialization.
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

            // The backend is ready and can now be ticked every frame.
            state = OnlineState::Initialized;
            RTB_INFO(std::string("OnlineSystem: initialized with backend ") + backend->GetName() + ".");
            return true;
        }

        void OnlineSystem::Tick(float deltaTime)
        {
            // Skip backend work unless initialization completed successfully.
            if (state != OnlineState::Initialized || !backend) {
                return;
            }

            // Forward frame updates to the active backend.
            backend->Tick(deltaTime);
        }

        void OnlineSystem::Shutdown()
        {
            // Shut down the active backend before clearing facade state.
            if (backend) {
                backend->Shutdown();
                backend.reset();
            }

            // Return the facade to its default disabled state.
            enabled = false;
            failApplicationOnError = false;
            defaultLoginOptions = OnlineLoginOptions();
            state = OnlineState::Disabled;
        }

        IOnlineIdentity* OnlineSystem::GetIdentity()
        {
            // Identity is only available while a backend instance exists.
            return backend ? backend->GetIdentity() : nullptr;
        }

        const IOnlineIdentity* OnlineSystem::GetIdentity() const
        {
            // Const overload for read-only diagnostics and tools.
            return backend ? backend->GetIdentity() : nullptr;
        }

        IOnlineLobby* OnlineSystem::GetLobby()
        {
            // Lobby is only available while a backend instance exists.
            return backend ? backend->GetLobby() : nullptr;
        }

        const IOnlineLobby* OnlineSystem::GetLobby() const
        {
            // Const overload for read-only diagnostics and tools.
            return backend ? backend->GetLobby() : nullptr;
        }

        IOnlineTransport* OnlineSystem::GetTransport()
        {
            // Transport is only available while a backend instance exists.
            return backend ? backend->GetTransport() : nullptr;
        }

        const IOnlineTransport* OnlineSystem::GetTransport() const
        {
            // Const overload for read-only diagnostics and tools.
            return backend ? backend->GetTransport() : nullptr;
        }

        // Centralized backend factory
        std::unique_ptr<IOnlineBackend> OnlineSystem::CreateBackend(OnlineBackendType type)
        {
            switch (type) {
            case OnlineBackendType::Null:
                // Null backend provides offline/test behavior.
                return std::make_unique<NullOnlineBackend>();
            case OnlineBackendType::EOS:
                // EOS backend owns the Epic Online Services SDK integration.
                return std::make_unique<EosOnlineBackend>();
            default:
                return nullptr;
            }
        }

    }
}
