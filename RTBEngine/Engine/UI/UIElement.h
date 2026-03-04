#pragma once
#include "../RTBEngineAPI.h"
#include "../ECS/Component.h"
#include "RectTransform.h"
#include "../Math/Vectors/Vector2.h"
#include <memory>

namespace RTBEngine {
	namespace UI {

		class RTB_API UIElement : public ECS::Component {
		public:
			UIElement();
			virtual ~UIElement();

			RectTransform* GetRectTransform() const { return rectTransform.get(); }

			void SyncRectTransform();

			void SetVisible(bool visible) { isVisible = visible; }
			bool IsVisible() const { return isVisible; }

			void SetRaycastTarget(bool value) { raycastTarget = value; }
			bool IsRaycastTarget() const { return raycastTarget; }

			virtual void OnAwake() override;
			virtual void OnUpdate(float deltaTime) override;

			virtual const char* GetTypeName() const override = 0;

			virtual void Render() = 0;

			// Reflected properties (Proxy)
			bool isVisible = true;
			bool raycastTarget = true;
			Math::Vector2 anchorMin = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 anchorMax = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 pivot = Math::Vector2(0.5f, 0.5f);
			Math::Vector2 anchoredPosition = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 sizeDelta = Math::Vector2(100.0f, 100.0f);

		protected:
			std::unique_ptr<RectTransform> rectTransform;
		};

	}
}
