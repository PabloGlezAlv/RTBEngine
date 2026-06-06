#include "CanvasSystem.h"
#include "Canvas.h"
#include "UIElement.h"
#include "UIRenderContext.h"
#include "Elements/UIImage.h"
#include "Elements/UIPanel.h"
#include "Elements/UIText.h"
#include "Elements/UIInputField.h"
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
#include "../Rendering/Font.h"
#include "../Rendering/Shader.h"
#include "../Rendering/Texture.h"
#include <GL/glew.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

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
					Draw(vertices.data(), vertices.size());
				}

				void Draw(const std::vector<WorldUIVertex>& vertices) {
					if (vertices.empty()) return;
					Draw(vertices.data(), vertices.size());
				}

			private:
				void Draw(const WorldUIVertex* vertices, size_t vertexCount) {
					EnsureInitialized();
					if (vao == 0 || vbo == 0 || !vertices || vertexCount == 0) return;

					// Upload the current UI mesh and draw it as a triangle list.
					glBindVertexArray(vao);
					glBindBuffer(GL_ARRAY_BUFFER, vbo);
					const GLsizeiptr bytes = static_cast<GLsizeiptr>(vertexCount * sizeof(WorldUIVertex));
					if (bytes > bufferCapacity) {
						glBufferData(GL_ARRAY_BUFFER, bytes, vertices, GL_DYNAMIC_DRAW);
						bufferCapacity = bytes;
					}
					else {
						glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, vertices);
					}
					glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
					glBindVertexArray(0);
				}

				void EnsureInitialized() {
					if (vao != 0 && vbo != 0) return;

					// One small dynamic vertex layout is enough for images, panels, and generated text quads.
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
				GLsizeiptr bufferCapacity = static_cast<GLsizeiptr>(sizeof(WorldUIVertex) * 6);
			};

			WorldUIQuadRenderer& GetWorldUIQuadRenderer() {
				static WorldUIQuadRenderer renderer;
				return renderer;
			}

			std::array<WorldUIVertex, 6> BuildWorldSpaceQuadFromRect(
				Canvas* canvas,
				const Math::Vector4& rect,
				float u0,
				float v0,
				float u1,
				float v1) {
				const Math::Vector2 canvasSize = canvas->GetCanvasSize();
				const float pixelsPerUnit = std::max(1.0f, canvas->GetPixelsPerUnit());

				// Convert the UI rect from canvas pixels into a local plane centered on the Canvas origin.
				const float x0 = (rect.x - canvasSize.x * 0.5f) / pixelsPerUnit;
				const float x1 = (rect.x + rect.z - canvasSize.x * 0.5f) / pixelsPerUnit;
				const float y0 = (canvasSize.y * 0.5f - rect.y) / pixelsPerUnit;
				const float y1 = (canvasSize.y * 0.5f - rect.y - rect.w) / pixelsPerUnit;

				return {{
					{ x0, y0, 0.0f, u0, v0 },
					{ x1, y0, 0.0f, u1, v0 },
					{ x1, y1, 0.0f, u1, v1 },
					{ x0, y0, 0.0f, u0, v0 },
					{ x1, y1, 0.0f, u1, v1 },
					{ x0, y1, 0.0f, u0, v1 },
				}};
			}

			std::array<WorldUIVertex, 6> BuildWorldSpaceQuad(Canvas* canvas, UIElement* element) {
				return BuildWorldSpaceQuadFromRect(
					canvas,
					element->GetRectTransform()->GetWorldRect(),
					0.0f,
					1.0f,
					1.0f,
					0.0f);
			}

			Math::Matrix4 BuildFaceCameraModelMatrix(ECS::GameObject* canvasObject, Rendering::Camera* camera) {
				const Math::Vector3 worldPos = canvasObject->GetWorldPosition();
				const Math::Vector3 worldScale = canvasObject->GetWorldScale();

				Math::Vector3 forward = camera->GetPosition() - worldPos;
				if (forward.LengthSquared() < 1e-8f) {
					forward = camera->GetForward();
				} else {
					forward = forward.Normalized();
				}

				Math::Vector3 upReference = Math::Vector3::Up();
				Math::Vector3 right = upReference.Cross(forward);
				if (right.LengthSquared() < 1e-8f) {
					upReference = Math::Vector3::Forward();
					right = upReference.Cross(forward);
				}
				right = right.Normalized();

				const Math::Vector3 up = forward.Cross(right).Normalized();

				Math::Matrix4 rotation = Math::Matrix4::Identity();
				rotation.m[0] = right.x;
				rotation.m[4] = right.y;
				rotation.m[8] = right.z;
				rotation.m[1] = up.x;
				rotation.m[5] = up.y;
				rotation.m[9] = up.z;
				rotation.m[2] = forward.x;
				rotation.m[6] = forward.y;
				rotation.m[10] = forward.z;

				return Math::Matrix4::Translate(worldPos) * rotation * Math::Matrix4::Scale(worldScale);
			}

			float GetEffectiveTextFontSize(UIText* text) {
				Math::Vector2 lossyScale = text->GetRectTransform()->GetLossyScale();
				// Text size follows UI scale so a scaled RectTransform keeps the same visual proportion.
				float effectiveScale = std::abs(lossyScale.y);
				if (effectiveScale < 0.01f) {
					effectiveScale = 0.01f;
				}

				return std::max(1.0f, text->GetFontSize() * effectiveScale);
			}

			Rendering::Font* ResolveTextFont(UIText* text) {
				Rendering::Font* activeFont = text->GetFont();
				if (!activeFont) {
					activeFont = Core::ResourceManager::GetInstance().GetDefaultFont();
				}

				return activeFont;
			}

			void RenderWorldSpaceText(Canvas* canvas, UIText* text, Rendering::Shader* shader) {
				if (text->GetText().empty()) return;

				const Math::Vector4 rect = text->GetRectTransform()->GetWorldRect();
				const float effectiveFontSize = GetEffectiveTextFontSize(text);
				Rendering::Font* activeFont = ResolveTextFont(text);
				if (!activeFont) return;

				const GLuint atlasTextureID = activeFont->GetAtlasTextureID(effectiveFontSize);
				if (atlasTextureID == 0) return;

				const std::string& value = text->GetText();
				Math::Vector2 textSize = activeFont->MeasureText(value, effectiveFontSize);

				float cursorX = rect.x;
				float textTopY = rect.y;
				switch (text->GetAlignment()) {
				case TextAlignment::Center:
					cursorX += (rect.z - textSize.x) * 0.5f;
					textTopY += (rect.w - textSize.y) * 0.5f;
					break;
				case TextAlignment::Right:
					cursorX += rect.z - textSize.x;
					textTopY += (rect.w - textSize.y) * 0.5f;
					break;
				case TextAlignment::Left:
				default:
					textTopY += (rect.w - textSize.y) * 0.5f;
					break;
				}

				float baselineY = textTopY;
				if (ImFont* imFont = activeFont->GetImFont(effectiveFontSize)) {
					if (ImFontBaked* bakedFont = imFont->GetFontBaked(effectiveFontSize)) {
						const float fontScale = effectiveFontSize / bakedFont->Size;
						baselineY += bakedFont->Ascent * fontScale;
					}
				}

				std::vector<Rendering::FontTextVertex> textVertices;
				activeFont->BuildTextGeometry(value, effectiveFontSize, Math::Vector2(cursorX, baselineY), textVertices);
				if (textVertices.empty()) return;

				const Math::Vector2 canvasSize = canvas->GetCanvasSize();
				const float pixelsPerUnit = std::max(1.0f, canvas->GetPixelsPerUnit());
				std::vector<WorldUIVertex> worldVertices;
				worldVertices.reserve(textVertices.size());
				// The font API gives us 2D canvas-space vertices; here we map them onto the 3D canvas plane.
				for (const Rendering::FontTextVertex& vertex : textVertices) {
					worldVertices.push_back({
						(vertex.x - canvasSize.x * 0.5f) / pixelsPerUnit,
						(canvasSize.y * 0.5f - vertex.y) / pixelsPerUnit,
						0.0f,
						vertex.u,
						vertex.v
					});
				}

				shader->SetBool("uHasTexture", true);
				shader->SetVector4("uColor", text->GetColor());
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, atlasTextureID);
				GetWorldUIQuadRenderer().Draw(worldVertices);
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			void RenderWorldSpaceElement(Canvas* canvas, UIElement* element, Rendering::Shader* shader) {
				if (!canvas || !element || !shader) return;

				ECS::GameObject* elementObject = element->GetOwner();
				if (!elementObject || !elementObject->IsActiveInHierarchy()) return;

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
					const Math::Vector4 bg = panel->GetBackgroundColor();
					if (bg.w <= 0.001f && !panel->hasBorder) {
						return;
					}

					shader->SetBool("uHasTexture", false);
					shader->SetVector4("uColor", panel->GetBackgroundColor());
					GetWorldUIQuadRenderer().Draw(BuildWorldSpaceQuad(canvas, element));
					return;
				}

				if (auto* text = dynamic_cast<UIText*>(element)) {
					RenderWorldSpaceText(canvas, text, shader);
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
				if (canvas && canvas->IsEnabled() && obj->IsActiveInHierarchy()) {
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

		void CanvasSystem::RenderToDrawList(ImDrawList* drawList, const Math::Vector2& renderScreenSize, const Math::Vector2& offset, float scale) {
			RenderToDrawList(drawList, renderScreenSize, offset, Math::Vector2(scale, scale));
		}

		void CanvasSystem::RenderToDrawList(ImDrawList* drawList, const Math::Vector2& renderScreenSize, const Math::Vector2& offset, const Math::Vector2& scale) {
			UIRenderContext::Begin(drawList, offset, scale);

			for (Canvas* canvas : activeCanvases) {
				// World-space canvases have their own 3D pass and must not be duplicated as HUD overlay.
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

			// Draw world UI as transparent scene geometry: depth-tested, alpha-blended, and double-sided.
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
				if (!canvasObject || !canvasObject->IsActiveInHierarchy()) continue;

				// World-space layout still starts from canvas pixels; only the final rendering happens in 3D.
				canvas->PrepareForHitTest(canvas->GetCanvasSize());
				const Math::Matrix4 modelMatrix = canvas->GetFaceCamera()
					? BuildFaceCameraModelMatrix(canvasObject, camera)
					: canvasObject->GetWorldMatrix();
				shader->SetMatrix4("uModel", modelMatrix);

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
			UIInputField::ClearFocusedField();
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
					ECS::GameObject* elementObject = element ? element->GetOwner() : nullptr;
					if (!elementObject || !elementObject->IsActiveInHierarchy()) continue;

					if (elementObject == gameObject && element->IsRaycastTarget()) {
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
					ECS::GameObject* elementObject = element ? element->GetOwner() : nullptr;
					if (!elementObject || !elementObject->IsActiveInHierarchy()) continue;

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
				UIInputField* inputField = currentGO ? currentGO->GetComponent<UIInputField>() : nullptr;
				if (!inputField || !inputField->IsInteractable()) {
					UIInputField::ClearFocusedField();
				}

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
