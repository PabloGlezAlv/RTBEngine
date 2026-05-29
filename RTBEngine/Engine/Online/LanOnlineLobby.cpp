#include "LanOnlineLobby.h"

#include "IOnlineIdentity.h"

#include <algorithm>
#include <array>
#include <random>
#include <sstream>

namespace {

    constexpr char kAdvertisePrefix[] = "RTB_AD ";     // host periodic LAN broadcast
    constexpr char kFindPrefix[] = "RTB_FIND ";         // client search by lobby code
    constexpr char kFoundPrefix[] = "RTB_FOUND ";       // host reply to FIND with game port
    constexpr char kJoinPrefix[] = "RTB_JOIN ";         // client requests membership
    constexpr char kJoinAckPrefix[] = "RTB_JOIN_ACK ";  // host confirms join + game port

    bool IsOperationInProgress(RTBEngine::Online::OnlineLobbyState state)
    {
        return state == RTBEngine::Online::OnlineLobbyState::Creating ||
            state == RTBEngine::Online::OnlineLobbyState::Searching ||
            state == RTBEngine::Online::OnlineLobbyState::Joining ||
            state == RTBEngine::Online::OnlineLobbyState::Leaving ||
            state == RTBEngine::Online::OnlineLobbyState::Destroying; // async UDP handshake in flight
    }

    std::string NormalizeLobbyCode(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::toupper(character)); // case-insensitive lobby codes
            });
        return value;
    }

}

namespace RTBEngine {
    namespace Online {

        LanOnlineLobby::LanOnlineLobby(IOnlineIdentity* identity, UdpNetworkTransport* transport)
            : identity(identity), transport(transport) // transport unused today; discovery uses discoverySocket
        {
        }

        bool LanOnlineLobby::Configure(std::uint16_t configuredDiscoveryPort, std::uint16_t configuredGamePort, std::string& outError)
        {
            discoveryPort = configuredDiscoveryPort; // shared LAN discovery port from OnlineConfig
            gamePort = configuredGamePort;           // gameplay port echoed in RTB_* payloads

            return BindDiscoverySocket(0, outError); // port 0 = OS assigns ephemeral bind (multi-instance safe)
        }

        void LanOnlineLobby::Tick(float deltaTime)
        {
            (void)deltaTime; // timer uses steady_clock, not deltaTime
            ProcessDiscoveryMessages(); // drain all pending discovery datagrams this frame

            if (state == OnlineLobbyState::InLobby && currentLobby.isOwner) {
                const auto now = std::chrono::steady_clock::now();
                if (lastAdvertiseTime.time_since_epoch().count() == 0 ||
                    now - lastAdvertiseTime >= std::chrono::milliseconds(500)) {
                    BroadcastLobbyAdvertisement(); // re-send RTB_AD every 500 ms while hosting
                    lastAdvertiseTime = now;
                }
            }
        }

        OnlineResult LanOnlineLobby::CreateLobby(const OnlineCreateLobbyOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot create a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (IsOperationInProgress(state)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Lobby operation already in progress.");
            }

            if (state == OnlineLobbyState::InLobby && !currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Already in a lobby.");
            }

            std::string bindError;
            if (!BindDiscoverySocket(discoveryPort, bindError)) {
                lastError = bindError;
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError); // host binds fixed discovery port
            }

            const std::uint32_t maxMembers = std::max<std::uint32_t>(2, options.maxMembers); // coop needs at least 2
            currentLobby = {};
            currentLobby.lobbyId = GenerateLobbyCode();              // random 6-char human code
            currentLobby.ownerUserId = identity->GetLocalUserId();   // Local:* on host machine
            currentLobby.memberUserIds = { currentLobby.ownerUserId };
            currentLobby.currentMembers = 1;
            currentLobby.maxMembers = maxMembers;
            currentLobby.availableSlots = maxMembers > 0 ? maxMembers - 1 : 0;
            currentLobby.isOwner = true; // this machine created the session
            searchResults.clear();
            lastError.clear();
            lastAdvertiseTime = {};
            SetState(OnlineLobbyState::InLobby);
            BroadcastLobbyAdvertisement(); // immediate first RTB_AD

            return OnlineResult::Success("Lobby created. Share your public IP, discovery port, and lobby code with remote players.");
        }

        bool LanOnlineLobby::BuildDiscoveryEndpoint(const std::string& hostAddress, UdpEndpoint& outEndpoint, std::string& outError) const
        {
            if (hostAddress.empty()) {
                outEndpoint = { "255.255.255.255", discoveryPort }; // LAN broadcast destination
                outError.clear();
                return true;
            }

            std::string resolvedHost;
            if (!ResolveHostAddress(hostAddress, resolvedHost, outError)) {
                return false; // DNS/literal resolution failed
            }

            outEndpoint.host = std::move(resolvedHost);
            outEndpoint.port = discoveryPort; // always talk to host's discovery port, not game port
            outError.clear();
            return true;
        }

        OnlineResult LanOnlineLobby::QueryLobbyAtHost(const std::string& hostAddress, const std::string& lobbyCode)
        {
            searchResults.clear();
            lastError.clear();
            SetState(OnlineLobbyState::Searching); // listening for RTB_FOUND / RTB_AD replies

            UdpEndpoint destination;
            std::string endpointError;
            if (!BuildDiscoveryEndpoint(hostAddress, destination, endpointError)) {
                SetState(OnlineLobbyState::Error);
                lastError = endpointError;
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, lastError);
            }

            const std::string request = std::string(kFindPrefix) + NormalizeLobbyCode(lobbyCode);
            if (!SendDiscoveryMessage(request, destination)) {
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            const auto start = std::chrono::steady_clock::now();
            const auto timeout = hostAddress.empty()
                ? std::chrono::milliseconds(750)   // LAN: short wait for broadcast replies
                : std::chrono::milliseconds(2000); // Internet: longer RTT tolerance
            while (std::chrono::steady_clock::now() - start < timeout) {
                ProcessDiscoveryMessages(); // busy-wait pump until timeout (blocking API)
            }

            SetState(OnlineLobbyState::NotInLobby); // search finished — not joined yet unless JoinLobby continues
            if (searchResults.empty()) {
                lastError = hostAddress.empty()
                    ? "No lobby found on the local network for that code."
                    : "No lobby found on host '" + hostAddress + "' for that code.";
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            return OnlineResult::Success("Lobby search completed.");
        }

        OnlineResult LanOnlineLobby::FindLobbies(const OnlineFindLobbiesOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot search lobbies without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            return QueryLobbyAtHost(options.hostAddress, options.lobbyId);
        }

        OnlineResult LanOnlineLobby::JoinLobby(const OnlineJoinLobbyOptions& options)
        {
            if (!identity || !identity->IsLoggedIn()) {
                lastError = "Cannot join a lobby without a logged-in local identity.";
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, lastError);
            }

            if (options.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby Id is required.");
            }

            const OnlineResult findResult = QueryLobbyAtHost(options.hostAddress, options.lobbyId);
            if (!findResult.success || searchResults.empty()) {
                return findResult; // propagate search failure
            }

            const OnlineLobbyInfo& foundLobby = searchResults.front(); // first matching host response
            UdpEndpoint hostEndpoint;
            if (!UdpEndpoint::TryParse(foundLobby.ownerUserId.GetValue(), hostEndpoint)) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidConfig, "Lobby host endpoint is invalid.");
            }

            hostEndpoint.port = discoveryPort; // JOIN goes to discovery port, not game port from FOUND
            SetState(OnlineLobbyState::Joining);

            const std::string joinMessage = std::string(kJoinPrefix) +
                identity->GetLocalUserId().GetValue() + "|" +  // Local id value without prefix
                identity->GetDisplayName() + "|" +
                std::to_string(gamePort); // tell host where to send gameplay packets
            if (!SendDiscoveryMessage(joinMessage, hostEndpoint)) {
                SetState(OnlineLobbyState::Error);
                return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
            }

            const auto start = std::chrono::steady_clock::now();
            const auto timeout = options.hostAddress.empty()
                ? std::chrono::milliseconds(750)
                : std::chrono::milliseconds(2000);
            while (std::chrono::steady_clock::now() - start < timeout) {
                ProcessDiscoveryMessages();
                if (state == OnlineLobbyState::InLobby) {
                    return OnlineResult::Success("Lobby joined."); // RTB_JOIN_ACK received
                }
            }

            SetState(OnlineLobbyState::Error);
            lastError = "Timed out waiting for lobby join acknowledgement.";
            return OnlineResult::Failure(OnlineErrorCode::BackendError, lastError);
        }

        OnlineResult LanOnlineLobby::LeaveLobby()
        {
            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to leave.");
            }

            currentLobby = {};
            searchResults.clear();
            lastError.clear();
            lastAdvertiseTime = {};
            SetState(OnlineLobbyState::NotInLobby); // local state only — remote not notified in V1
            return OnlineResult::Success("LAN lobby left.");
        }

        OnlineResult LanOnlineLobby::DestroyLobby()
        {
            if (currentLobby.lobbyId.empty()) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "No active lobby to destroy.");
            }

            if (!currentLobby.isOwner) {
                return OnlineResult::Failure(OnlineErrorCode::InvalidState, "Only the lobby owner can destroy the lobby.");
            }

            currentLobby = {};
            searchResults.clear();
            lastError.clear();
            lastAdvertiseTime = {};
            SetState(OnlineLobbyState::NotInLobby);
            return OnlineResult::Success("LAN lobby destroyed.");
        }

        OnlineLobbyState LanOnlineLobby::GetState() const
        {
            return state;
        }

        const OnlineLobbyInfo& LanOnlineLobby::GetCurrentLobby() const
        {
            return currentLobby; // empty lobbyId when not in lobby
        }

        const std::vector<OnlineLobbyInfo>& LanOnlineLobby::GetSearchResults() const
        {
            return searchResults; // populated after FindLobbies / QueryLobbyAtHost
        }

        const char* LanOnlineLobby::GetLastError() const
        {
            return lastError.c_str();
        }

        Core::EventSubscription LanOnlineLobby::SubscribeLobbyStatusChanged(Core::Event<OnlineLobbyStatusChangedEvent>::Callback callback)
        {
            return lobbyStatusChanged.Subscribe(std::move(callback));
        }

        void LanOnlineLobby::ClearLobbyStatusChangedListeners()
        {
            lobbyStatusChanged.Clear();
        }

        void LanOnlineLobby::SetState(OnlineLobbyState newState)
        {
            const OnlineLobbyState previousState = state;
            state = newState;

            if (previousState == newState) {
                return;
            }

            lobbyStatusChanged.Invoke({ previousState, state, currentLobby }); // UI refreshes member list
        }

        std::string LanOnlineLobby::GenerateLobbyCode() const
        {
            static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no O/0/I/1 confusion
            std::random_device device;
            std::mt19937 generator(device());
            std::uniform_int_distribution<std::size_t> distribution(0, sizeof(kAlphabet) - 2);

            std::string code;
            code.reserve(6);
            for (int i = 0; i < 6; ++i) {
                code.push_back(kAlphabet[distribution(generator)]);
            }

            return code;
        }

        void LanOnlineLobby::ProcessDiscoveryMessages()
        {
            std::array<char, 512> buffer{}; // discovery messages are short ASCII text
            while (true) {
                UdpEndpoint sender;
                std::uint32_t bytesRead = 0;
                std::string receiveError;
                if (!discoverySocket.ReceiveFrom(buffer.data(), static_cast<std::uint32_t>(buffer.size()), bytesRead, sender, receiveError)) {
                    break; // no more datagrams in kernel queue
                }

                const std::string message(buffer.data(), buffer.data() + bytesRead); // whole datagram as string

                if (message.rfind(kAdvertisePrefix, 0) == 0 && state == OnlineLobbyState::Searching) {
                    const std::string payload = message.substr(std::strlen(kAdvertisePrefix));
                    const std::size_t separator = payload.find('|');
                    if (separator == std::string::npos) {
                        continue; // malformed RTB_AD
                    }

                    OnlineLobbyInfo result;
                    result.lobbyId = payload.substr(0, separator);
                    result.ownerUserId = OnlineUserId(OnlineUserIdType::NetworkPeer, sender.ToAddress()); // host IP from UDP source
                    result.memberUserIds = { result.ownerUserId };
                    result.currentMembers = 1;
                    result.maxMembers = 6;
                    result.availableSlots = 5;
                    result.isOwner = false;
                    searchResults.push_back(result);
                    continue;
                }

                if (message.rfind(kFindPrefix, 0) == 0 &&
                    state == OnlineLobbyState::InLobby &&
                    currentLobby.isOwner) {
                    const std::string requestedCode = NormalizeLobbyCode(message.substr(std::strlen(kFindPrefix)));
                    if (requestedCode != currentLobby.lobbyId) {
                        continue; // FIND for a different lobby — ignore
                    }

                    const std::string response = std::string(kFoundPrefix) + currentLobby.lobbyId + "|" +
                        std::to_string(gamePort); // embed gameplay port in FOUND reply
                    SendDiscoveryMessage(response, sender); // unicast back to searcher
                    continue;
                }

                if (message.rfind(kFoundPrefix, 0) == 0 && state == OnlineLobbyState::Searching) {
                    const std::string payload = message.substr(std::strlen(kFoundPrefix));
                    const std::size_t separator = payload.find('|');
                    const std::string lobbyCode = separator == std::string::npos
                        ? payload
                        : payload.substr(0, separator);

                    UdpEndpoint ownerEndpoint = sender; // default: host IP from packet source
                    if (separator != std::string::npos) {
                        try {
                            const int remoteGamePort = std::stoi(payload.substr(separator + 1));
                            if (remoteGamePort > 0 && remoteGamePort <= 65535) {
                                ownerEndpoint.port = static_cast<std::uint16_t>(remoteGamePort); // override with advertised game port
                            }
                        }
                        catch (...) {
                        }
                    }

                    OnlineLobbyInfo result;
                    result.lobbyId = lobbyCode;
                    result.ownerUserId = OnlineUserId(OnlineUserIdType::NetworkPeer, ownerEndpoint.ToAddress());
                    result.memberUserIds = { result.ownerUserId };
                    result.currentMembers = 1;
                    result.maxMembers = 6;
                    result.availableSlots = 5;
                    result.isOwner = false;
                    searchResults.push_back(result);
                    continue;
                }

                if (message.rfind(kJoinPrefix, 0) == 0 &&
                    state == OnlineLobbyState::InLobby &&
                    currentLobby.isOwner) {
                    const std::string payload = message.substr(std::strlen(kJoinPrefix));
                    const std::size_t firstSeparator = payload.find('|');
                    const std::size_t secondSeparator = payload.find('|', firstSeparator == std::string::npos ? 0 : firstSeparator + 1);
                    if (firstSeparator == std::string::npos || secondSeparator == std::string::npos) {
                        continue; // expected: memberId|displayName|clientGamePort
                    }

                    const std::string memberId = payload.substr(0, firstSeparator);
                    const std::string clientGamePortText = payload.substr(secondSeparator + 1);
                    UdpEndpoint clientEndpoint = sender; // client IP from UDP source address
                    try {
                        const int clientGamePort = std::stoi(clientGamePortText);
                        if (clientGamePort > 0 && clientGamePort <= 65535) {
                            clientEndpoint.port = static_cast<std::uint16_t>(clientGamePort); // client's gameplay bind port
                        }
                    }
                    catch (...) {
                        continue;
                    }

                    const OnlineUserId remoteMember(OnlineUserIdType::NetworkPeer, clientEndpoint.ToAddress());
                    if (std::find(currentLobby.memberUserIds.begin(), currentLobby.memberUserIds.end(), remoteMember) ==
                        currentLobby.memberUserIds.end()) {
                        currentLobby.memberUserIds.push_back(remoteMember); // host now knows where to SendPacket
                        ++currentLobby.currentMembers;
                        if (currentLobby.availableSlots > 0) {
                            --currentLobby.availableSlots;
                        }
                    }

                    const std::string ack = std::string(kJoinAckPrefix) + currentLobby.lobbyId + "|" +
                        memberId + "|" + std::to_string(gamePort); // host gameplay port for client
                    SendDiscoveryMessage(ack, sender);
                    continue;
                }

                if (message.rfind(kJoinAckPrefix, 0) == 0 && state == OnlineLobbyState::Joining) {
                    const std::string payload = message.substr(std::strlen(kJoinAckPrefix));
                    const std::size_t firstSeparator = payload.find('|');
                    const std::size_t secondSeparator = payload.find('|', firstSeparator == std::string::npos ? 0 : firstSeparator + 1);
                    if (firstSeparator == std::string::npos) {
                        continue;
                    }

                    currentLobby.lobbyId = payload.substr(0, firstSeparator);
                    UdpEndpoint hostEndpoint = sender;
                    if (secondSeparator != std::string::npos) {
                        try {
                            const int remoteGamePort = std::stoi(payload.substr(secondSeparator + 1));
                            if (remoteGamePort > 0 && remoteGamePort <= 65535) {
                                hostEndpoint.port = static_cast<std::uint16_t>(remoteGamePort);
                            }
                        }
                        catch (...) {
                        }
                    }
                    else {
                        hostEndpoint.port = gamePort; // fallback if ack omits port field
                    }

                    currentLobby.ownerUserId = OnlineUserId(OnlineUserIdType::NetworkPeer, hostEndpoint.ToAddress()); // SendToHost target
                    currentLobby.memberUserIds = { currentLobby.ownerUserId, identity->GetLocalUserId() };
                    currentLobby.currentMembers = 2;
                    currentLobby.maxMembers = 6;
                    currentLobby.availableSlots = 4;
                    currentLobby.isOwner = false; // joined someone else's lobby
                    SetState(OnlineLobbyState::InLobby);
                }
            }
        }

        void LanOnlineLobby::BroadcastLobbyAdvertisement()
        {
            if (currentLobby.lobbyId.empty()) {
                return;
            }

            const std::string message = std::string(kAdvertisePrefix) + currentLobby.lobbyId + "|" +
                std::to_string(gamePort);
            UdpEndpoint broadcastEndpoint{ "255.255.255.255", discoveryPort };
            SendDiscoveryMessage(message, broadcastEndpoint);
        }

        bool LanOnlineLobby::SendDiscoveryMessage(const std::string& message, const UdpEndpoint& destination)
        {
            std::string sendError;
            if (!discoverySocket.SendTo(message.data(), static_cast<std::uint32_t>(message.size()), destination, sendError)) {
                lastError = sendError;
                return false;
            }

            lastError.clear();
            return true;
        }

        bool LanOnlineLobby::BindDiscoverySocket(std::uint16_t bindPort, std::string& outError)
        {
            discoverySocket.Close();
            if (!discoverySocket.Open("0.0.0.0", bindPort, true, outError)) { // broadcast enabled for RTB_AD
                return false;
            }

            outError.clear();
            return true;
        }

    }
}
