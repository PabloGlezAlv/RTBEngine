#include "Canvas.h"
#include "../ECS/GameObject.h"
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>
#include <SDL.h>
#include <algorithm>

namespace RTBEngine {
	namespace UI {

		Canvas::Canvas() {
			canvasSize = Math::Vector2(1920.0f, 1080.0f);
		}

		Canvas::~Canvas() {
			Cleanup();
		}

		void Canvas::Initialize(SDL_Window* sdlWindow) {
			if (isInitialized) return;

			window = sdlWindow;

			// Get screen size from SDL window
			int width, height;
			SDL_GetWindowSize(window, &width, &height);
			canvasSize = Math::Vector2(static_cast<float>(width), static_cast<float>(height));

			// Initialize ImGui context (only once globally)
			static bool imguiInitialized = false;
			if (!imguiInitialized) {
				IMGUI_CHECKVERSION();
				ImGui::CreateContext();
				ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

				ImGui::StyleColorsDark();

				ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext());
				ImGui_ImplOpenGL3_Init("#version 330");

				imguiInitialized = true;
			}

			isInitialized = true;
		}

		void Canvas::Cleanup() {
			if (!isInitialized) return;

			cachedUIElements.clear();
			isInitialized = false;
		}

		void Canvas::OnAwake() {
			// Canvas will be initialized by Application
		}

		void Canvas::OnStart() {
			CollectUIElements();
		}

		void Canvas::OnUpdate(float deltaTime) {
			// Update canvas size in case window was resized
			if (window && renderMode == RenderMode::ScreenSpaceOverlay) {
				int width, height;
				SDL_GetWindowSize(window, &width, &height);
				canvasSize = Math::Vector2(static_cast<float>(width), static_cast<float>(height));
			}

			// Recollect UI elements (in case children were added/removed)
			CollectUIElements();
		}

		void Canvas::OnDestroy() {
			Cleanup();
		}

		void Canvas::BeginFrame() {
			if (!isInitialized) return;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
		}

		void Canvas::RenderCanvas() {
			if (!isInitialized) return;

			UpdateRectTransforms();

			// Render all UI elements that are children of this Canvas
			for (UIElement* element : cachedUIElements) {
				if (element && element->IsVisible() && element->IsEnabled()) {
					element->Render();
				}
			}
		}

		void Canvas::EndFrame() {
			if (!isInitialized) return;

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		void Canvas::CollectUIElements() {
			cachedUIElements.clear();

			if (!owner) return;

			// Get all children GameObjects
			const auto& children = owner->GetChildren();
			for (ECS::GameObject* child : children) {
				// Check if child has a UIElement component
				UIElement* uiElem = child->GetComponent<UIElement>();
				if (uiElem) {
					cachedUIElements.push_back(uiElem);
				}

				// Recursively check grandchildren
				const auto& grandchildren = child->GetChildren();
				for (ECS::GameObject* grandchild : grandchildren) {
					UIElement* grandElem = grandchild->GetComponent<UIElement>();
					if (grandElem) {
						cachedUIElements.push_back(grandElem);
					}
				}
			}
		}

		void Canvas::UpdateRectTransforms() {
			// Update all RectTransforms with parent hierarchy
			for (UIElement* element : cachedUIElements) {
				if (!element) continue;

				RectTransform* rt = element->GetRectTransform();
				if (!rt) continue;

				// Get parent RectTransform if exists
				ECS::GameObject* parentObj = element->GetOwner()->GetParent();
				Math::Vector2 parentPos(0.0f, 0.0f);
				Math::Vector2 parentSize = canvasSize;

				if (parentObj && parentObj != owner) {
					UIElement* parentUI = parentObj->GetComponent<UIElement>();
					if (parentUI && parentUI->GetRectTransform()) {
						parentPos = parentUI->GetRectTransform()->GetScreenPosition();
						parentSize = parentUI->GetRectTransform()->GetScreenSize();
					}
				}

				rt->CalculateScreenRect(parentPos, parentSize);
			}
		}

	}
}
