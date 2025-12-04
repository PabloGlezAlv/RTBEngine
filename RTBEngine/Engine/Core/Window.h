#pragma once
#include <SDL.h>
#include "GL/glew.h"
#include <string>

namespace RTBEngine {
    namespace Core {

        class Window {
        public:
			Window(const std::string& title, int width, int height);

			~Window();

			bool Initialize();

			void SwapBuffers();

			void Shutdown();

			SDL_Window* GetSDLWindow() const { return sdlWindow; }

			int GetWidth() const { return width; }
			int GetHeight() const { return height; }

			bool GetShouldClose() const { return shouldClose; }
			void SetShouldClose(bool value) { shouldClose = value; }
        private:
			std::string title = "";
			int width = 0;
			int height = 0;

			bool shouldClose = false;

			SDL_Window* sdlWindow;
			SDL_GLContext glContext;

			Window(const Window&) = delete;
			Window& operator=(const Window&) = delete;
        };

    } 
}