#include "Application.h"
#include <GL/glew.h> 

RTBEngine::Core::Application::Application() : window(nullptr), lastTime(0), isRunning(false)
{
}

RTBEngine::Core::Application::~Application()
{
	Shutdown();
}

bool RTBEngine::Core::Application::Initialize()
{
	window = new Window("RTBEngine - Application", 800, 600);
	if (!window->Initialize()) {
		return false;
	}

	lastTime = SDL_GetTicks();

	return true;
}

void RTBEngine::Core::Application::Run()
{
	isRunning = true;

	while (isRunning)
	{
		ProcessInput();

		Uint32 currentTime = SDL_GetTicks();
		float deltaTime = (currentTime - lastTime) / 1000.0f;
		lastTime = currentTime;

		Update(deltaTime);

		Render();
	}
}

void RTBEngine::Core::Application::Shutdown()
{
	isRunning = false;

	if (window)
	{
		delete window;
		window = nullptr;
	}
}

void RTBEngine::Core::Application::ProcessInput()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			isRunning = false;
	}

}

void RTBEngine::Core::Application::Update(float deltaTime)
{


}

void RTBEngine::Core::Application::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	window->SwapBuffers();
}
