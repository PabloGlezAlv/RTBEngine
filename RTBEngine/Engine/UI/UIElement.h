#pragma once
#include "../ECS/Component.h"
#include "RectTransform.h"
#include <memory>

namespace RTBEngine {
	namespace UI {

		class UIElement : public ECS::Component {
		public:
			UIElement();
			virtual ~UIElement();

			RectTransform* GetRectTransform() const { return rectTransform.get(); }

			// Reflected in subclasses
			bool isVisible = true;

			void SetVisible(bool visible) { isVisible = visible; }
			bool IsVisible() const { return isVisible; }

			virtual void OnAwake() override;
			virtual void OnUpdate(float deltaTime) override;

			virtual const char* GetTypeName() const override = 0;

			virtual void Render() = 0;

		protected:
			std::unique_ptr<RectTransform> rectTransform;
		};

	}
}
