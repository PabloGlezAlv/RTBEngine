#include "CanvasSystem.h"
#include "Canvas.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../Core/ResourceManager.h"
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>
#include <SDL.h>
#include <algorithm>
#include <iostream>

namespace RTBEngine {
	namespace UI {

		bool CanvasSystem::Initialize(SDL_Window* sdlWindow) {
			if (isInitialized) return true;

			if (!sdlWindow) {
				std::cerr << "CanvasSystem::Initialize - Invalid SDL_Window" << std::endl;
				return false;
			}

			window = sdlWindow;

			int width, height;
			SDL_GetWindowSize(window, &width, &height);
			screenSize = Math::Vector2(static_cast<float>(width), static_cast<float>(height));

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			if (!ImGui::GetCurrentContext()) {
				std::cerr << "CanvasSystem::Initialize - Failed to create ImGui context" << std::endl;
				return false;
			}

			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

			ImGui::StyleColorsDark();

			if (!ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext())) {
				std::cerr << "CanvasSystem::Initialize - Failed to initialize ImGui SDL2 backend" << std::endl;
				return false;
			}

			if (!ImGui_ImplOpenGL3_Init("#version 330")) {
				std::cerr << "CanvasSystem::Initialize - Failed to initialize ImGui OpenGL3 backend" << std::endl;
				ImGui_ImplSDL2_Shutdown();
				return false;
			}

			InitializeFonts();

			isInitialized = true;
			return true;
		}

		void CanvasSystem::Shutdown() {
			if (!isInitialized) return;

			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplSDL2_Shutdown();
			ImGui::DestroyContext();

			activeCanvases.clear();
			isInitialized = false;
		}

		void CanvasSystem::Update(ECS::Scene* scene) {
			if (!scene) return;

			activeCanvases.clear();

			int width, height;
			SDL_GetWindowSize(window, &width, &height);
			screenSize = Math::Vector2(static_cast<float>(width), static_cast<float>(height));

			for (const auto& objPtr : scene->GetGameObjects()) {
				ECS::GameObject* obj = objPtr.get();
				Canvas* canvas = obj->GetComponent<Canvas>();
				if (canvas && canvas->IsEnabled() && obj->IsActive()) {
					activeCanvases.push_back(canvas);
				}
			}

			std::sort(activeCanvases.begin(), activeCanvases.end(),
				[](Canvas* a, Canvas* b) {
					return a->GetSortOrder() < b->GetSortOrder();
				});
		}

		void CanvasSystem::RenderAll() {
			if (!isInitialized) return;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			for (Canvas* canvas : activeCanvases) {
				canvas->RenderCanvas(screenSize);
			}

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		void CanvasSystem::InitializeFonts() {
			Core::ResourceManager::GetInstance().GetDefaultFont();
		}

	}
}
