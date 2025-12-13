#include "Window.h"
#include <iostream>

RTBEngine::Core::Window::Window(const std::string& title, int width, int height) : title(title),
width(width),
height(height),
sdlWindow(nullptr),
glContext(nullptr),
shouldClose(false)
{
	
}

RTBEngine::Core::Window::~Window()
{
	Shutdown();
}

bool RTBEngine::Core::Window::Initialize()
{
	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "Error: Failed to initialize SDL2: " << SDL_GetError() << std::endl;
		return false;
	}

	//Initialize OpenGL version
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	//Create window
	sdlWindow = SDL_CreateWindow(
		title.c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
	);

	if (!sdlWindow) {
		std::cerr << "Error: Failed to create window: " << SDL_GetError() << std::endl;
		return false;
	}

	//Create OpenGL context
	glContext = SDL_GL_CreateContext(sdlWindow);

	if (!glContext) {
		std::cerr << "Error: Failed to create OpenGL context: " << SDL_GetError() << std::endl;
		return false;
	}

	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK) {
		std::cerr << "Error: Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
		return false;
	}

	// Enable OpenGL features
	glEnable(GL_DEPTH_TEST);        // Enable depth testing
	glDepthFunc(GL_LESS);            // Depth test passes if fragment is closer
	glEnable(GL_CULL_FACE);          // Enable face culling
	glCullFace(GL_BACK);             // Cull back faces
	glFrontFace(GL_CCW);             // Front faces are counter-clockwise

	SDL_GL_SetSwapInterval(1); // ENABLE V-SYNC

	return true;
}

void RTBEngine::Core::Window::SwapBuffers()
{
	SDL_GL_SwapWindow(sdlWindow);
}

void RTBEngine::Core::Window::Shutdown()
{
	if (glContext)
	{
		SDL_GL_DeleteContext(glContext);
		glContext = nullptr;
	}

	if (sdlWindow)
	{
		SDL_DestroyWindow(sdlWindow);
		sdlWindow = nullptr;
	}

	SDL_Quit();
}
