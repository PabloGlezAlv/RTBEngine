#pragma once

#include "../RTBEngineAPI.h"
#include "Component.h"
#include "../Online/OnlineGameplayNet.h"
#include "../Reflection/PropertyMacros.h"

namespace RTBEngine {
    namespace ECS {

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API NetworkTransform : public Component {
        public:
            NetworkTransform() = default;
            ~NetworkTransform() override = default;

            float sendRate = 20.0f;
            float interpolationSpeed = 14.0f;
            bool replicatePosition = true;
            bool replicateRotation = true;

            RTB_COMPONENT(NetworkTransform)

        public:
            void OnStart() override;
            void OnFixedUpdate(float fixedDeltaTime) override;
            void OnLateUpdate(float deltaTime) override;
            void OnValidate() override;
            void OnDestroy() override;

        private:
            float sendTimer = 0.0f;
            std::uint32_t registeredNetworkId = Online::OnlineGameplayNet::kInvalidNetworkObjectId;
            bool hasCachedSnapshot = false;
            bool loggedMissingNetworkIdError = false;
            Online::OnlineGameplayNet::TransformSnapshot cachedSnapshot;

            std::uint32_t ResolveNetworkId() const;
            void EnsureNetworkIdRegistered();
            bool HasSendAuthority() const;
            bool HasReceiveAuthority() const;
            void SendSnapshot(float deltaTime);
            void ApplyRemoteSnapshot(float deltaTime);
        };
#pragma warning(pop)

    }
}
