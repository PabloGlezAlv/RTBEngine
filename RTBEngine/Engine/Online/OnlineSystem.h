#pragma once

#include "../RTBEngineAPI.h"
#include "IOnlineBackend.h"
#include "IOnlineIdentity.h"
#include "IOnlineLobby.h"
#include "OnlineTypes.h"

#include <memory>
#include <string>

namespace RTBEngine {
    namespace Online {

        struct OnlineConfig;
#pragma warning(push)
#pragma warning(disable: 4251)
        // Engine-facing online facade.
        class RTB_API OnlineSystem {
        public:
            static OnlineSystem& GetInstance();

            bool Initialize(const OnlineConfig& config);
            void Tick(float deltaTime);
            void Shutdown();

            // Lightweight state queries for tools, gameplay code, and diagnostics.
            bool IsEnabled() const { return enabled; }
            bool IsInitialized() const { return state == OnlineState::Initialized; }
            OnlineState GetState() const { return state; }
            OnlineBackendType GetBackendType() const { return backendType; }
            const std::string& GetLastError() const { return lastError; }
            IOnlineIdentity* GetIdentity();
            const IOnlineIdentity* GetIdentity() const;
            IOnlineLobby* GetLobby();
            const IOnlineLobby* GetLobby() const;

        private:
            OnlineSystem() = default;
            ~OnlineSystem();

            OnlineSystem(const OnlineSystem&) = delete;
            OnlineSystem& operator=(const OnlineSystem&) = delete;

            std::unique_ptr<IOnlineBackend> CreateBackend(OnlineBackendType type);

            bool enabled = false;
            bool failApplicationOnError = false;
            OnlineState state = OnlineState::Disabled;
            OnlineBackendType backendType = OnlineBackendType::EOS;
            std::string lastError;
            std::unique_ptr<IOnlineBackend> backend;
        };
#pragma warning(pop)

    }
}
