#include "CanvasSystem.h"
#include "Canvas.h"
#include "UIElement.h"
#include "UIRenderContext.h"
#include "EventSystem/IPointerEnterHandler.h"
#include "EventSystem/IPointerExitHandler.h"
#include "EventSystem/IPointerDownHandler.h"
#include "EventSystem/IPointerUpHandler.h"
#include "EventSystem/IPointerClickHandler.h"
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
#include "../RTBEngine.h"

namespace RTBEngine {
	namespace UI {

		bool CanvasSystem::Initialize(SDL_Window* sdlWindow) {
			if (isInitialized) return true;

			if (!sdlWindow) {
				RTB_ERROR("CanvasSystem::Initialize - Invalid SDL_Window");
				return false;
			}

			window = sdlWindow;

			int width, height;
			SDL_GetWindowSize(window, &width, &height);
			screenSize = Math::Vector2(static_cast<float>(width), static_cast<float>(height));

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			if (!ImGui::GetCurrentContext()) {
				RTB_ERROR("CanvasSystem::Initialize - Failed to create ImGui context");
				return false;
			}

			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

			ImGui::StyleColorsDark();

			if (!ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext())) {
				RTB_ERROR("CanvasSystem::Initialize - Failed to initialize ImGui SDL2 backend");
				return false;
			}

			if (!ImGui_ImplOpenGL3_Init("#version 330")) {
				RTB_ERROR("CanvasSystem::Initialize - Failed to initialize ImGui OpenGL3 backend");
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

		void CanvasSystem::RenderCanvasesOnly(const Math::Vector2* customScreenSize) {
			if (!isInitialized) return;

			// Use custom screen size if provided, otherwise use stored size
			Math::Vector2 renderSize = customScreenSize ? *customScreenSize : screenSize;

			for (Canvas* canvas : activeCanvases) {
				canvas->RenderCanvas(renderSize);
			}
		}

		void CanvasSystem::RenderCanvasesToDrawList(void* drawList, const Math::Vector2& screenSize, const Math::Vector2& offset) {
			if (!isInitialized) return;

			// Set the render context so UI elements use the provided DrawList and offset
			UIRenderContext::Begin(static_cast<ImDrawList*>(drawList), offset);

			for (Canvas* canvas : activeCanvases) {
				canvas->RenderCanvas(screenSize);
			}

			UIRenderContext::End();
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

		template<typename THandler, typename TCallback>
		void CanvasSystem::ExecuteEvents(ECS::GameObject* target, const PointerEventData& eventData, TCallback callback) {
			if (!target) return;

			for (const auto& comp : target->GetComponents()) {
				THandler* handler = dynamic_cast<THandler*>(comp.get());
				if (handler) {
					callback(handler, eventData);
				}
			}
		}

		void CanvasSystem::ProcessInput() {
			if (!isInitialized) return;

			Input::InputManager& input = Input::InputManager::GetInstance();
			Math::Vector2 mousePos = GetMousePosition();

			UIElement* elementUnderMouse = GetElementUnderMouse(mousePos);
			ECS::GameObject* currentGO = elementUnderMouse ? elementUnderMouse->GetOwner() : nullptr;

			PointerEventData eventData;
			eventData.position = mousePos;
			eventData.pointerEnter = currentGO;

			if (hoveredGameObject != currentGO) {
				if (hoveredGameObject) {
					ExecuteEvents<IPointerExitHandler>(hoveredGameObject, eventData,
						[](IPointerExitHandler* h, const PointerEventData& e) { h->OnPointerExit(e); });
				}

				if (currentGO) {
					ExecuteEvents<IPointerEnterHandler>(currentGO, eventData,
						[](IPointerEnterHandler* h, const PointerEventData& e) { h->OnPointerEnter(e); });
				}

				hoveredGameObject = currentGO;
			}

			if (input.IsMouseButtonJustPressed(Input::MouseButton::Left)) {
				if (currentGO) {
					pressedGameObject = currentGO;
					eventData.pointerPress = currentGO;
					ExecuteEvents<IPointerDownHandler>(currentGO, eventData,
						[](IPointerDownHandler* h, const PointerEventData& e) { h->OnPointerDown(e); });
				}
			}

			if (input.IsMouseButtonJustReleased(Input::MouseButton::Left)) {
				if (currentGO) {
					ExecuteEvents<IPointerUpHandler>(currentGO, eventData,
						[](IPointerUpHandler* h, const PointerEventData& e) { h->OnPointerUp(e); });

					if (pressedGameObject == currentGO) {
						ExecuteEvents<IPointerClickHandler>(currentGO, eventData,
							[](IPointerClickHandler* h, const PointerEventData& e) { h->OnPointerClick(e); });
					}
				}
				pressedGameObject = nullptr;
			}
		}

	}
}
