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
            case OnlineLobbyState::InLobby:
                return "InLobby";
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
