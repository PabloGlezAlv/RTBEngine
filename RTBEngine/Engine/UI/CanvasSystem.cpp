#include "CanvasSystem.h"
#include "Canvas.h"
#include "UIElement.h"
#include "Elements/UIButton.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../Core/ResourceManager.h"
#include "../Input/InputManager.h"
#include "../Input/MouseButton.h"
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

		Math::Vector2 CanvasSystem::GetMousePosition() const {
			int x, y;
			SDL_GetMouseState(&x, &y);
			return Math::Vector2(static_cast<float>(x), static_cast<float>(y));
		}

		bool CanvasSystem::IsPointInRect(const Math::Vector2& point, const Math::Vector4& rect) {
			return point.x >= rect.x && point.x <= rect.x + rect.z &&
				   point.y >= rect.y && point.y <= rect.y + rect.w;
		}

		UIElement* CanvasSystem::GetElementUnderMouse(const Math::Vector2& mousePos) {
			for (auto it = activeCanvases.rbegin(); it != activeCanvases.rend(); ++it) {
				Canvas* canvas = *it;
				const auto& elements = canvas->GetUIElements();

				for (auto elemIt = elements.rbegin(); elemIt != elements.rend(); ++elemIt) {
					UIElement* element = *elemIt;
					if (!element->IsVisible()) continue;

					Math::Vector4 screenRect = element->GetRectTransform()->GetScreenRect();
					if (IsPointInRect(mousePos, screenRect)) {
						return element;
					}
				}
			}
			return nullptr;
		}

		void CanvasSystem::ProcessInput() {
			if (!isInitialized) return;

			Input::InputManager& input = Input::InputManager::GetInstance();
			Math::Vector2 mousePos = GetMousePosition();

			UIElement* elementUnderMouse = GetElementUnderMouse(mousePos);
			
			// Buscar UIButton en el mismo GameObject que el elemento visual encontrado
			UIButton* buttonUnderMouse = nullptr;
			if (elementUnderMouse && elementUnderMouse->GetOwner()) {
				buttonUnderMouse = elementUnderMouse->GetOwner()->GetComponent<UIButton>();
			}

			if (hoveredButton != buttonUnderMouse) {
				if (hoveredButton) {
					hoveredButton->OnPointerExit();
				}
				hoveredButton = buttonUnderMouse;
				if (hoveredButton) {
					hoveredButton->OnPointerEnter();
				}
			}

			if (input.IsMouseButtonJustPressed(Input::MouseButton::Left)) {
				if (buttonUnderMouse) {
					pressedButton = buttonUnderMouse;
					pressedButton->OnPointerDown();
				}
			}

			if (input.IsMouseButtonJustReleased(Input::MouseButton::Left)) {
				if (pressedButton) {
					if (pressedButton == buttonUnderMouse) {
						pressedButton->OnPointerUp();
					}
					else {
						pressedButton->OnPointerExit();
					}
					pressedButton = nullptr;
				}
			}
		}

	}
}
