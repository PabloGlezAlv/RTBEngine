#pragma once
#include "../RTBEngineAPI.h"
#include <SDL.h>
#include <string>
#include <functional>
#include "../Rendering/RHI/GraphicsAPI.h"

namespace RTBEngine {
    namespace Core {

        class RTB_API Window {
        public:
			Window(const std::string& title, int width, int height, bool fullscreen = false, bool maximized = false);

			~Window();

			// Creates the OS window. Graphics context is created later by IRenderDevice.
			bool Initialize(Rendering::RHI::GraphicsAPI graphicsAPI = Rendering::RHI::GraphicsAPI::OpenGL);

			void SwapBuffers();

			void Shutdown();

			SDL_Window* GetSDLWindow() const { return sdlWindow; }

			int GetWidth() const { return width; }
			int GetHeight() const { return height; }

			bool GetShouldClose() const { return shouldClose; }
			void SetShouldClose(bool value) { shouldClose = value; }

			void SetFullscreen(bool enabled);
			bool IsFullscreen() const { return isFullscreen; }

			void SetMouseCaptured(bool captured);
			bool IsMouseCaptured() const;

			void SetCursorVisible(bool visible);
			bool IsCursorVisible() const;

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

			SDL_Window* sdlWindow = nullptr;
			Rendering::RHI::GraphicsAPI graphicsAPI = Rendering::RHI::GraphicsAPI::OpenGL;

			WindowResizeCallback resizeCallback = nullptr;

			Window(const Window&) = delete;
			Window& operator=(const Window&) = delete;
        };

    } 
}
