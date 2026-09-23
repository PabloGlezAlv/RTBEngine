#include "Window.h"
#include "../Rendering/RHI/RenderDevice.h"
#include "../RTBEngine.h"
#include "../../ThirdParty/stb/stb_image.h"

#include <cstring>

RTBEngine::Core::Window::Window(const std::string& title, int width, int height, bool fullscreen, bool maximized) : title(title),
width(width),
height(height),
fullscreen(fullscreen),
maximized(maximized),
sdlWindow(nullptr),
shouldClose(false)
{
}

RTBEngine::Core::Window::~Window()
{
	Shutdown();
}

bool RTBEngine::Core::Window::Initialize(Rendering::RHI::GraphicsAPI api)
{
	graphicsAPI = api;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		RTB_ERROR("Error: Failed to initialize SDL2: " + std::string(SDL_GetError()));
		return false;
	}

	Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	if (graphicsAPI == Rendering::RHI::GraphicsAPI::Vulkan) {
		// Vulkan creates its own surface via SDL_Vulkan_CreateSurface; no GL attributes needed.
		windowFlags |= SDL_WINDOW_VULKAN;
	}
	else {
		// GL attributes must be set before SDL_CreateWindow for OpenGL windows.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		windowFlags |= SDL_WINDOW_OPENGL;
	}

	if (maximized) {
		windowFlags |= SDL_WINDOW_MAXIMIZED;
	}

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

	if (fullscreen) {
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		isFullscreen = true;
	}

	SDL_GetWindowSize(sdlWindow, &width, &height);
	return true;
}

void RTBEngine::Core::Window::SwapBuffers()
{
	if (Rendering::RHI::RenderDevice::HasDevice()) {
		Rendering::RHI::RenderDevice::Get().Present();
		return;
	}

	if (sdlWindow) {
		SDL_GL_SwapWindow(sdlWindow);
	}
}

void RTBEngine::Core::Window::Shutdown()
{
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
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else {
		SDL_SetWindowFullscreen(sdlWindow, 0);
	}

	isFullscreen = enabled;
}

void RTBEngine::Core::Window::SetMouseCaptured(bool captured)
{
	if (!sdlWindow || IsMouseCaptured() == captured) {
		return;
	}

	SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
	SDL_ShowCursor(captured ? SDL_DISABLE : SDL_ENABLE);

	isMouseCaptured = IsMouseCaptured();
	isCursorVisible = IsCursorVisible();
}

void RTBEngine::Core::Window::SetCursorVisible(bool visible)
{
	if (!sdlWindow) {
		isCursorVisible = visible;
		return;
	}

	if (IsCursorVisible() == visible) {
		return;
	}

	SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
	isCursorVisible = IsCursorVisible();
}

bool RTBEngine::Core::Window::IsMouseCaptured() const
{
	if (!sdlWindow) {
		return isMouseCaptured;
	}

	return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

bool RTBEngine::Core::Window::IsCursorVisible() const
{
	if (!sdlWindow) {
		return isCursorVisible;
	}

	return SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
}

bool RTBEngine::Core::Window::SetIcon(const std::string& imagePath)
{
	if (!sdlWindow || imagePath.empty()) {
		return false;
	}

	int imageWidth = 0;
	int imageHeight = 0;
	int channels = 0;
	unsigned char* pixels = stbi_load(imagePath.c_str(), &imageWidth, &imageHeight, &channels, 4);
	if (!pixels || imageWidth <= 0 || imageHeight <= 0) {
		if (pixels) {
			stbi_image_free(pixels);
		}
		RTB_WARN("Window icon not loaded: " + imagePath);
		return false;
	}

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, imageWidth, imageHeight, 32, SDL_PIXELFORMAT_RGBA32);
	if (!surface) {
		stbi_image_free(pixels);
		RTB_WARN("Window icon surface failed: " + std::string(SDL_GetError()));
		return false;
	}

	if (SDL_MUSTLOCK(surface)) {
		SDL_LockSurface(surface);
	}
	std::memcpy(surface->pixels, pixels, static_cast<std::size_t>(imageWidth) * static_cast<std::size_t>(imageHeight) * 4);
	if (SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}

	stbi_image_free(pixels);
	SDL_SetWindowIcon(sdlWindow, surface);
	SDL_FreeSurface(surface);
	return true;
}

void RTBEngine::Core::Window::UpdateSize(int newWidth, int newHeight)
{
	width = newWidth;
	height = newHeight;

	if (resizeCallback) {
		resizeCallback(width, height);
	}
}
