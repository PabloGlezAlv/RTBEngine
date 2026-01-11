#pragma once
#include <SDL.h>
#include "GL/glew.h"
#include <string>
#include <functional>

namespace RTBEngine {
    namespace Core {

        class Window {
        public:
			Window(const std::string& title, int width, int height, bool fullscreen = false, bool maximized = false);

			~Window();

			bool Initialize();

			void SwapBuffers();

			void Shutdown();

			SDL_Window* GetSDLWindow() const { return sdlWindow; }

			int GetWidth() const { return width; }
			int GetHeight() const { return height; }

			bool GetShouldClose() const { return shouldClose; }
			void SetShouldClose(bool value) { shouldClose = value; }

			// Fullscreen control
			void SetFullscreen(bool enabled);
			bool IsFullscreen() const { return isFullscreen; }

			// Mouse capture control
			void SetMouseCaptured(bool captured);
			bool IsMouseCaptured() const { return isMouseCaptured; }

			// Cursor visibility control
			void SetCursorVisible(bool visible);
			bool IsCursorVisible() const { return isCursorVisible; }

			void UpdateSize(int newWidth, int newHeight);

			using WindowResizeCallback = std::function<void(int, int)>;
			void SetResizeCallback(WindowResizeCallback callback) { resizeCallback = callback; }
        private:
			std::string title = "";
			int width = 0;
			int height = 0;
			bool fullscreen = false;
			bool maximized = false;

			bool shouldClose = false;

			bool isFullscreen = false;
			bool isMouseCaptured = false;
			bool isCursorVisible = true;

			SDL_Window* sdlWindow;
			SDL_GLContext glContext;

			WindowResizeCallback resizeCallback = nullptr;

			Window(const Window&) = delete;
			Window& operator=(const Window&) = delete;
        };

    } 
}