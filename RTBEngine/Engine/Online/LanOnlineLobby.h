#pragma once

#include "IOnlineLobby.h"
#include "UdpNetworkTransport.h"
#include "UdpSocket.h"

#include <chrono>
#include <string>

namespace RTBEngine {
    namespace Online {

        class IOnlineIdentity;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API LanOnlineLobby final : public IOnlineLobby {
        public:
            LanOnlineLobby(IOnlineIdentity* identity, UdpNetworkTransport* transport);

            bool Configure(std::uint16_t discoveryPort, std::uint16_t gamePort, std::string& outError);
            void Tick(float deltaTime);

            OnlineResult CreateLobby(const OnlineCreateLobbyOptions& options) override;
            OnlineResult FindLobbies(const OnlineFindLobbiesOptions& options) override;
            OnlineResult JoinLobby(const OnlineJoinLobbyOptions& options) override;
            OnlineResult LeaveLobby() override;
            OnlineResult DestroyLobby() override;
            OnlineLobbyState GetState() const override;
            const OnlineLobbyInfo& GetCurrentLobby() const override;
            const std::vector<OnlineLobbyInfo>& GetSearchResults() const override;
            const char* GetLastError() const override;
            Core::EventSubscription SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback) override;
            void ClearLobbyStatusChangedListeners() override;

        private:
            void SetState(OnlineLobbyState newState);
            std::string GenerateLobbyCode() const;
            void ProcessDiscoveryMessages();
            void BroadcastLobbyAdvertisement();
            bool SendDiscoveryMessage(const std::string& message, const UdpEndpoint& destination);
            bool BindDiscoverySocket(std::uint16_t bindPort, std::string& outError);
            bool BuildDiscoveryEndpoint(const std::string& hostAddress, UdpEndpoint& outEndpoint, std::string& outError) const;
            OnlineResult QueryLobbyAtHost(const std::string& hostAddress, const std::string& lobbyCode);

            IOnlineIdentity* identity = nullptr;
            UdpNetworkTransport* transport = nullptr;
            UdpSocket discoverySocket;
            std::uint16_t discoveryPort = 27016;
            std::uint16_t gamePort = 27015;
            OnlineLobbyState state = OnlineLobbyState::NotInLobby;
            OnlineLobbyInfo currentLobby;
            std::vector<OnlineLobbyInfo> searchResults;
            std::string lastError;
            std::chrono::steady_clock::time_point lastAdvertiseTime{};
            Core::Event<OnlineLobbyStatusChangedEvent> lobbyStatusChanged;
        };
#pragma warning(pop)

    }
}
