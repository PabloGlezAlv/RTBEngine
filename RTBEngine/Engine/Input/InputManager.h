#pragma once
#include <SDL.h>
#include <unordered_map>
#include "KeyCode.h"
#include "MouseButton.h"

namespace RTBEngine {
    namespace Input {

        class InputManager {
        public:
            static InputManager& GetInstance();

            void Update();
            void ProcessEvent(const SDL_Event& event);

            bool IsKeyPressed(KeyCode key) const;
            bool IsKeyJustPressed(KeyCode key) const;
            bool IsKeyJustReleased(KeyCode key) const;

            bool IsMouseButtonPressed(MouseButton button) const;
            bool IsMouseButtonJustPressed(MouseButton button) const;
            bool IsMouseButtonJustReleased(MouseButton button) const;

            int GetMouseX() const { return mouseX; }
            int GetMouseY() const { return mouseY; }
            int GetMouseDeltaX() const { return mouseDeltaX; }
            int GetMouseDeltaY() const { return mouseDeltaY; }
            int GetScrollDelta() const { return scrollDelta; }

            void SetMouseRelativeMode(bool enabled);
            void SetMousePosition(int x, int y);

        private:
            InputManager();
            ~InputManager();

            InputManager(const InputManager&) = delete;
            InputManager& operator=(const InputManager&) = delete;

            KeyCode SDLKeyToKeyCode(SDL_Keycode sdlKey) const;

            std::unordered_map<KeyCode, bool> currentKeys;
            std::unordered_map<KeyCode, bool> previousKeys;

            std::unordered_map<MouseButton, bool> currentMouseButtons;
            std::unordered_map<MouseButton, bool> previousMouseButtons;

            int mouseX;
            int mouseY;
            int mouseDeltaX;
            int mouseDeltaY;
            int scrollDelta;
        };

    }
}