#include <SDL.h>
#include <GL/glew.h>
#include <iostream>

int main(int argc, char* argv[]) {
    // Inicializar SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Error: SDL2 - " << SDL_GetError() << std::endl;
        return -1;
    }

    // OpenGL 4.3 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Crear ventana
    SDL_Window* window = SDL_CreateWindow(
        "RTBEngine - Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cout << "Error: Ventana - " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Contexto OpenGL
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cout << "Error: OpenGL Context - " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Inicializar GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cout << "Error: GLEW!" << std::endl;
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    std::cout << "=== RTBEngine Test ===" << std::endl;
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLEW: " << glewGetString(GLEW_VERSION) << std::endl;

    // Fondo azul
    glClearColor(0.2f, 0.4f, 0.8f, 1.0f);

    // Loop 3 segundos
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) break;
        }
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    // Limpieza
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "Test completado!" << std::endl;
    return 0;
}