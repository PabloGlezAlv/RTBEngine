#include "OnlineSystem.h"

#include "LanOnlineBackend.h"
#include "IOnlineBackend.h"
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

        bool OnlineSystem::Initialize(const OnlineConfig& config)
        {
            Shutdown();

            enabled = config.enabled;
            failApplicationOnError = config.failApplicationOnError;
            defaultLoginOptions.type = config.loginType;
            defaultLoginOptions.displayName = config.loginDisplayName;
            lastError.clear();

            if (!enabled) {
                state = OnlineState::Disabled;
                RTB_INFO("OnlineSystem: disabled.");
                return true;
            }

            backend = std::make_unique<LanOnlineBackend>();

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
            defaultLoginOptions = OnlineLoginOptions();
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

        IOnlineLobby* OnlineSystem::GetLobby()
        {
            return backend ? backend->GetLobby() : nullptr;
        }

        const IOnlineLobby* OnlineSystem::GetLobby() const
        {
            return backend ? backend->GetLobby() : nullptr;
        }

        IOnlineTransport* OnlineSystem::GetTransport()
        {
            return backend ? backend->GetTransport() : nullptr;
        }

        const IOnlineTransport* OnlineSystem::GetTransport() const
        {
            return backend ? backend->GetTransport() : nullptr;
        }

    }
}
