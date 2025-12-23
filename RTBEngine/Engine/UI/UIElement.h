#pragma once
#include "../ECS/Component.h"
#include "RectTransform.h"
#include <memory>
#include <functional>

namespace RTBEngine {
	namespace UI {

		class UIElement : public ECS::Component {
		public:
			UIElement();
			virtual ~UIElement();

			// RectTransform access
			RectTransform* GetRectTransform() const { return rectTransform.get(); }

			// Visibility
			void SetVisible(bool visible) { isVisible = visible; }
			bool IsVisible() const { return isVisible; }

			// Interactivity
			void SetInteractable(bool interactable) { isInteractable = interactable; }
			bool IsInteractable() const { return isInteractable; }

			// UI Event callbacks (override in derived classes)
			virtual void OnPointerEnter() {}
			virtual void OnPointerExit() {}
			virtual void OnPointerDown() {}
			virtual void OnPointerUp() {}

			// Lifecycle override
			virtual void OnAwake() override;
			virtual void OnUpdate(float deltaTime) override;

			virtual const char* GetTypeName() const override = 0;

			virtual void Render() = 0;

		protected:
			std::unique_ptr<RectTransform> rectTransform;
			bool isVisible = true;
			bool isInteractable = true;
			bool isHovered = false;
		};

	}
}
