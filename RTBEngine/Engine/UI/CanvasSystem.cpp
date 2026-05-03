#include "CanvasSystem.h"
#include "Canvas.h"
#include "UIElement.h"
#include "UIRenderContext.h"
#include "Elements/UIImage.h"
#include "Elements/UIPanel.h"
#include "EventSystem/IPointerEnterHandler.h"
#include "EventSystem/IPointerExitHandler.h"
#include "EventSystem/IPointerDownHandler.h"
#include "EventSystem/IPointerUpHandler.h"
#include "EventSystem/IPointerClickHandler.h"
#include "EventSystem/IBeginDragHandler.h"
#include "EventSystem/IDragHandler.h"
#include "EventSystem/IEndDragHandler.h"
#include "../ECS/Scene.h"
#include "../ECS/GameObject.h"
#include "../Core/ResourceManager.h"
#include "../Input/InputManager.h"
#include "../Input/MouseButton.h"
#include "../Rendering/Camera.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include <GL/glew.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstddef>

namespace RTBEngine {
	namespace UI {

		namespace {
			struct WorldUIVertex {
				float x;
				float y;
				float z;
				float u;
				float v;
			};

			class WorldUIQuadRenderer {
			public:
				void Draw(const std::array<WorldUIVertex, 6>& vertices) {
					EnsureInitialized();
					if (vao == 0 || vbo == 0) return;

					glBindVertexArray(vao);
					glBindBuffer(GL_ARRAY_BUFFER, vbo);
					glBufferSubData(GL_ARRAY_BUFFER, 0,
						static_cast<GLsizeiptr>(vertices.size() * sizeof(WorldUIVertex)),
						vertices.data());
					glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
					glBindVertexArray(0);
				}

			private:
				void EnsureInitialized() {
					if (vao != 0 && vbo != 0) return;

					glGenVertexArrays(1, &vao);
					glGenBuffers(1, &vbo);

					glBindVertexArray(vao);
					glBindBuffer(GL_ARRAY_BUFFER, vbo);
					glBufferData(GL_ARRAY_BUFFER,
						static_cast<GLsizeiptr>(sizeof(WorldUIVertex) * 6),
						nullptr,
						GL_DYNAMIC_DRAW);

					glEnableVertexAttribArray(0);
					glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
						sizeof(WorldUIVertex),
						reinterpret_cast<void*>(offsetof(WorldUIVertex, x)));

					glEnableVertexAttribArray(1);
					glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
						sizeof(WorldUIVertex),
						reinterpret_cast<void*>(offsetof(WorldUIVertex, u)));

					glBindVertexArray(0);
				}

				GLuint vao = 0;
				GLuint vbo = 0;
			};

			WorldUIQuadRenderer& GetWorldUIQuadRenderer() {
				static WorldUIQuadRenderer renderer;
				return renderer;
			}

			std::array<WorldUIVertex, 6> BuildWorldSpaceQuad(Canvas* canvas, UIElement* element) {
				const Math::Vector4 rect = element->GetRectTransform()->GetWorldRect();
				const Math::Vector2 canvasSize = canvas->GetCanvasSize();
				const float pixelsPerUnit = std::max(1.0f, canvas->GetPixelsPerUnit());

				const float x0 = (rect.x - canvasSize.x * 0.5f) / pixelsPerUnit;
				const float x1 = (rect.x + rect.z - canvasSize.x * 0.5f) / pixelsPerUnit;
				const float y0 = (canvasSize.y * 0.5f - rect.y) / pixelsPerUnit;
				const float y1 = (canvasSize.y * 0.5f - rect.y - rect.w) / pixelsPerUnit;

				return {{
					{ x0, y0, 0.0f, 0.0f, 1.0f },
					{ x1, y0, 0.0f, 1.0f, 1.0f },
					{ x1, y1, 0.0f, 1.0f, 0.0f },
					{ x0, y0, 0.0f, 0.0f, 1.0f },
					{ x1, y1, 0.0f, 1.0f, 0.0f },
					{ x0, y1, 0.0f, 0.0f, 0.0f },
				}};
			}

			void RenderWorldSpaceElement(Canvas* canvas, UIElement* element, Rendering::Shader* shader) {
				if (!canvas || !element || !shader) return;
				if (!element->IsVisible() || !element->IsEnabled()) return;

				const Math::Vector4 rect = element->GetRectTransform()->GetWorldRect();
				if (rect.z <= 0.0f || rect.w <= 0.0f) return;

				if (auto* image = dynamic_cast<UIImage*>(element)) {
					Rendering::Texture* texture = image->GetTexture();
					if (!texture || texture->GetID() == 0) return;

					shader->SetBool("uHasTexture", true);
					shader->SetVector4("uColor", image->GetTint());
					texture->Bind(0);
					GetWorldUIQuadRenderer().Draw(BuildWorldSpaceQuad(canvas, element));
					texture->Unbind();
					return;
				}

				if (auto* panel = dynamic_cast<UIPanel*>(element)) {
					shader->SetBool("uHasTexture", false);
					shader->SetVector4("uColor", panel->GetBackgroundColor());
					GetWorldUIQuadRenderer().Draw(BuildWorldSpaceQuad(canvas, element));
				}
			}
		}

		void CanvasSystem::Update(ECS::Scene* scene) {
			if (!scene) return;

			activeScene = scene;
			activeCanvases.clear();

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

		void CanvasSystem::UpdateAllRectTransforms(const Math::Vector2& customScreenSize) {
			screenSize = customScreenSize;
			for (Canvas* canvas : activeCanvases) {
				canvas->PrepareForHitTest(customScreenSize);
			}
		}

		void CanvasSystem::RenderToDrawList(ImDrawList* drawList, const Math::Vector2& renderScreenSize, const Math::Vector2& offset) {
			UIRenderContext::Begin(drawList, offset);

			for (Canvas* canvas : activeCanvases) {
				if (canvas->GetRenderMode() == Canvas::RenderMode::WorldSpace) continue;
				canvas->RenderCanvas(renderScreenSize);
			}

			UIRenderContext::End();
		}

		void CanvasSystem::RenderWorldSpace(Rendering::Camera* camera) {
			if (!camera) return;

			Rendering::Shader* shader = Core::ResourceManager::GetInstance().GetShader("ui_world");
			if (!shader) return;

			GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
			GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
			GLboolean wasCullFaceEnabled = glIsEnabled(GL_CULL_FACE);
			GLboolean wasDepthMaskEnabled = GL_TRUE;
			GLint previousBlendSrcRgb = GL_ONE;
			GLint previousBlendDstRgb = GL_ZERO;
			GLint previousBlendSrcAlpha = GL_ONE;
			GLint previousBlendDstAlpha = GL_ZERO;
			glGetBooleanv(GL_DEPTH_WRITEMASK, &wasDepthMaskEnabled);
			glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
			glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
			glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
			glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_CULL_FACE);

			shader->Bind();
			shader->SetMatrix4("uView", camera->GetViewMatrix());
			shader->SetMatrix4("uProjection", camera->GetProjectionMatrix());
			shader->SetInt("uTexture", 0);

			for (Canvas* canvas : activeCanvases) {
				if (!canvas || canvas->GetRenderMode() != Canvas::RenderMode::WorldSpace) continue;
				ECS::GameObject* canvasObject = canvas->GetOwner();
				if (!canvasObject || !canvasObject->IsActive()) continue;

				canvas->PrepareForHitTest(canvas->GetCanvasSize());
				shader->SetMatrix4("uModel", canvasObject->GetWorldMatrix());

				for (UIElement* element : canvas->GetUIElements()) {
					RenderWorldSpaceElement(canvas, element, shader);
				}
			}

			shader->Unbind();

			glDepthMask(wasDepthMaskEnabled);
			if (wasBlendEnabled) {
				glEnable(GL_BLEND);
			} else {
				glDisable(GL_BLEND);
			}
			glBlendFuncSeparate(previousBlendSrcRgb, previousBlendDstRgb, previousBlendSrcAlpha, previousBlendDstAlpha);

			if (wasCullFaceEnabled) {
				glEnable(GL_CULL_FACE);
			} else {
				glDisable(GL_CULL_FACE);
			}

			if (wasDepthTestEnabled) {
				glEnable(GL_DEPTH_TEST);
			} else {
				glDisable(GL_DEPTH_TEST);
			}
		}

		void CanvasSystem::ClearState() {
			activeCanvases.clear();
			activeScene = nullptr;
			hoveredGameObject = nullptr;
			pressedGameObject = nullptr;
			draggingGameObject = nullptr;
		}

		bool CanvasSystem::IsGameObjectAlive(ECS::GameObject* gameObject) const {
			if (!activeScene || !gameObject) return false;
			for (const auto& obj : activeScene->GetGameObjects()) {
				if (obj.get() == gameObject) return true;
			}
			return false;
		}

		std::vector<Math::Vector4> CanvasSystem::GetRaycastRectsForGameObject(ECS::GameObject* gameObject) const {
			std::vector<Math::Vector4> rects;
			if (!gameObject) return rects;
			for (Canvas* canvas : activeCanvases) {
				if (canvas->GetRenderMode() == Canvas::RenderMode::WorldSpace) continue;
				for (UIElement* element : canvas->GetUIElements()) {
					if (element->GetOwner() == gameObject && element->IsRaycastTarget()) {
						rects.push_back(element->GetRectTransform()->GetWorldRect());
					}
				}
			}
			return rects;
		}

		bool CanvasSystem::IsPointInRect(const Math::Vector2& point, const Math::Vector4& rect) {
			return point.x >= rect.x && point.x <= rect.x + rect.z &&
				   point.y >= rect.y && point.y <= rect.y + rect.w;
		}

		UIElement* CanvasSystem::GetElementUnderMouse(const Math::Vector2& mousePos) {
			for (auto it = activeCanvases.rbegin(); it != activeCanvases.rend(); ++it) {
				Canvas* canvas = *it;
				if (canvas->GetRenderMode() == Canvas::RenderMode::WorldSpace) continue;
				const auto& elements = canvas->GetUIElements();

				for (auto elemIt = elements.rbegin(); elemIt != elements.rend(); ++elemIt) {
					UIElement* element = *elemIt;
					if (!element->IsVisible() || !element->IsEnabled() || !element->IsRaycastTarget()) continue;

					Math::Vector4 screenRect = element->GetRectTransform()->GetWorldRect();
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

		void CanvasSystem::ProcessInput(const Math::Vector2& mousePos) {
			if (!IsGameObjectAlive(hoveredGameObject)) hoveredGameObject = nullptr;
			if (!IsGameObjectAlive(pressedGameObject)) pressedGameObject = nullptr;
			if (!IsGameObjectAlive(draggingGameObject)) draggingGameObject = nullptr;

			Input::InputManager& input = Input::InputManager::GetInstance();

			UIElement* elementUnderMouse = GetElementUnderMouse(mousePos);
			ECS::GameObject* currentGO = elementUnderMouse ? elementUnderMouse->GetOwner() : nullptr;

			PointerEventData eventData;
			eventData.position = mousePos;
			eventData.delta = Math::Vector2(
				static_cast<float>(input.GetMouseDeltaX()),
				static_cast<float>(input.GetMouseDeltaY()));
			eventData.pointerEnter = currentGO;
			eventData.pointerPress = pressedGameObject;
			eventData.button = static_cast<int>(Input::MouseButton::Left);

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
					draggingGameObject = currentGO;
					eventData.pointerPress = currentGO;
					ExecuteEvents<IPointerDownHandler>(currentGO, eventData,
						[](IPointerDownHandler* h, const PointerEventData& e) { h->OnPointerDown(e); });
					ExecuteEvents<IBeginDragHandler>(currentGO, eventData,
						[](IBeginDragHandler* h, const PointerEventData& e) { h->OnBeginDrag(e); });
				}
			}

			if (draggingGameObject &&
				input.IsMouseButtonPressed(Input::MouseButton::Left) &&
				!input.IsMouseButtonJustPressed(Input::MouseButton::Left)) {
				eventData.pointerPress = draggingGameObject;
				ExecuteEvents<IDragHandler>(draggingGameObject, eventData,
					[](IDragHandler* h, const PointerEventData& e) { h->OnDrag(e); });
			}

			if (input.IsMouseButtonJustReleased(Input::MouseButton::Left)) {
				eventData.pointerPress = pressedGameObject;

				if (draggingGameObject) {
					PointerEventData dragEndData = eventData;
					dragEndData.pointerPress = draggingGameObject;
					ExecuteEvents<IEndDragHandler>(draggingGameObject, dragEndData,
						[](IEndDragHandler* h, const PointerEventData& e) { h->OnEndDrag(e); });
				}

				ECS::GameObject* releaseTarget = pressedGameObject ? pressedGameObject : currentGO;
				if (releaseTarget) {
					ExecuteEvents<IPointerUpHandler>(releaseTarget, eventData,
						[](IPointerUpHandler* h, const PointerEventData& e) { h->OnPointerUp(e); });
				}

				if (pressedGameObject && pressedGameObject == currentGO) {
					ExecuteEvents<IPointerClickHandler>(pressedGameObject, eventData,
						[](IPointerClickHandler* h, const PointerEventData& e) { h->OnPointerClick(e); });
				}
				pressedGameObject = nullptr;
				draggingGameObject = nullptr;
			}
		}

	}
}
