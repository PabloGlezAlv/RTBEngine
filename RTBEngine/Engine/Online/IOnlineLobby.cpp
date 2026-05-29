#include "IOnlineLobby.h"

namespace RTBEngine {
    namespace Online {

        const char* ToString(OnlineLobbyState state)
        {
            switch (state) {
            case OnlineLobbyState::NotInLobby:
                return "NotInLobby";
            case OnlineLobbyState::Creating:
                return "Creating";
            case OnlineLobbyState::Searching:
                return "Searching";   // RTB_FIND in flight
            case OnlineLobbyState::Joining:
                return "Joining";     // waiting for RTB_JOIN_ACK
            case OnlineLobbyState::InLobby:
                return "InLobby";     // session active
            case OnlineLobbyState::Leaving:
                return "Leaving";
            case OnlineLobbyState::Destroying:
                return "Destroying";
            case OnlineLobbyState::Error:
                return "Error";
            default:
                return "Unknown";
            }
        }

    }
}
