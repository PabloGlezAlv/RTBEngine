#include "Window.h"
#include <iostream>
#include "../RTBEngine.h"

RTBEngine::Core::Window::Window(const std::string& title, int width, int height, bool fullscreen, bool maximized) : title(title),
width(width),
height(height),
fullscreen(fullscreen),
maximized(maximized),
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
		RTB_ERROR("Error: Failed to initialize SDL2: " + std::string(SDL_GetError()));
		return false;
	}

	//Initialize OpenGL version
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	if (maximized) {
		windowFlags |= SDL_WINDOW_MAXIMIZED;
	}

	//Create window
	sdlWindow = SDL_CreateWindow(
		title.c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		width, height,
		windowFlags
	);

	if (!sdlWindow) {
		RTB_ERROR("Error: Failed to create window: " + std::string(SDL_GetError()));
		return false;
	}

	//Create OpenGL context
	glContext = SDL_GL_CreateContext(sdlWindow);

	if (!glContext) {
		RTB_ERROR("Error: Failed to create OpenGL context: " + std::string(SDL_GetError()));
		return false;
	}

	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK) {
		RTB_ERROR("Error: Failed to initialize GLEW: " + std::string((const char*)glewGetErrorString(glewError)));
		return false;
	}

	// Enable OpenGL features
	glEnable(GL_DEPTH_TEST);        // Enable depth testing
	glDepthFunc(GL_LESS);            // Depth test passes if fragment is closer
	glEnable(GL_CULL_FACE);          // Enable face culling
	glCullFace(GL_BACK);             // Cull back faces
	glFrontFace(GL_CCW);             // Front faces are counter-clockwise

	SDL_GL_SetSwapInterval(1); // ENABLE V-SYNC

	// Apply fullscreen if configured
	if (fullscreen) {
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		isFullscreen = true;
	}

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

void RTBEngine::Core::Window::SetFullscreen(bool enabled)
{
	if (!sdlWindow || isFullscreen == enabled) {
		return;
	}

	if (enabled) {
		// Fullscreen borderless mode (keeps desktop resolution)
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else {
		// Return to windowed mode
		SDL_SetWindowFullscreen(sdlWindow, 0);
	}

	isFullscreen = enabled;
}

void RTBEngine::Core::Window::SetMouseCaptured(bool captured)
{
	if (!sdlWindow || isMouseCaptured == captured) {
		return;
	}

	// Relative mode: hides cursor and captures mouse movement
	SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);

	// Also control cursor visibility
	SDL_ShowCursor(captured ? SDL_DISABLE : SDL_ENABLE);

	isMouseCaptured = captured;
	isCursorVisible = !captured;
}

void RTBEngine::Core::Window::SetCursorVisible(bool visible)
{
	if (isCursorVisible == visible) {
		return;
	}

	SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
	isCursorVisible = visible;
}

void RTBEngine::Core::Window::UpdateSize(int newWidth, int newHeight)
{
	width = newWidth;
	height = newHeight;

	if (resizeCallback) {
		resizeCallback(width, height);
	}
}
