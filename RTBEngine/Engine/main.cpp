#include "Core/Window.h"
#include <iostream>
#include <GL/glew.h>

int main(int argc, char* argv[]) {

    // Crear ventana
    RTBEngine::Core::Window window("RTBEngine - Window Test", 800, 600);

    // Inicializar
    if (!window.Initialize()) {
        std::cerr << "Failed to initialize window!" << std::endl;
        return -1;
    }

    glClearColor(0.2f, 0.4f, 0.8f, 1.0f);


    SDL_Event event;
    while (!window.GetShouldClose()) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                window.SetShouldClose(true);
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                window.SetShouldClose(true);
            }
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Intercambiar buffers
        window.SwapBuffers();
    }


    return 0;
}