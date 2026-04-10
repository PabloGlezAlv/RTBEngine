#pragma once
#include "../RTBEngineAPI.h"
#include "../ECS/Component.h"
#include "RectTransform.h"
#include "../Math/Vectors/Vector2.h"
#include <memory>

namespace RTBEngine {
	namespace UI {

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API UIElement : public ECS::Component {
		public:
			UIElement();
			virtual ~UIElement();

			RectTransform* GetRectTransform() const { return rectTransform.get(); }

			void SyncRectTransform();
			void PropagateDirtyToChildren();

			void SetVisible(bool visible) { isVisible = visible; }
			bool IsVisible() const { return isVisible; }

			void SetRaycastTarget(bool value) { raycastTarget = value; }
			bool IsRaycastTarget() const { return raycastTarget; }

			// Transform proxy setters auto-sync into RectTransform and propagate dirty.
			void SetAnchorMin(const Math::Vector2& value);
			void SetAnchorMax(const Math::Vector2& value);
			void SetPivot(const Math::Vector2& value);
			void SetAnchoredPosition(const Math::Vector2& value);
			void SetSizeDelta(const Math::Vector2& value);
			void SetRotation(float degrees);
			void SetScale(const Math::Vector2& value);

			Math::Vector2 GetAnchorMin() const { return anchorMin; }
			Math::Vector2 GetAnchorMax() const { return anchorMax; }
			Math::Vector2 GetPivot() const { return pivot; }
			Math::Vector2 GetAnchoredPosition() const { return anchoredPosition; }
			Math::Vector2 GetSizeDelta() const { return sizeDelta; }
			float GetRotation() const { return rotation; }
			Math::Vector2 GetScale() const { return scale; }

			virtual void OnAwake() override;
			virtual void OnParentChanged(ECS::GameObject* oldParent, ECS::GameObject* newParent) override;

			virtual const char* GetTypeName() const override = 0;

			virtual void Render() = 0;

			// Reflected properties that are intentionally editable at runtime.
			bool isVisible = true;
			bool raycastTarget = true;

		protected:
			std::unique_ptr<RectTransform> rectTransform;

		private:
			// Runtime transform state is synchronized through setters and parent-change hooks,
			// so it no longer needs a per-frame OnUpdate repair path.
			Math::Vector2 anchorMin = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 anchorMax = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 pivot = Math::Vector2(0.5f, 0.5f);
			Math::Vector2 anchoredPosition = Math::Vector2(0.0f, 0.0f);
			Math::Vector2 sizeDelta = Math::Vector2(100.0f, 100.0f);
			float rotation = 0.0f;
			Math::Vector2 scale = Math::Vector2(1.0f, 1.0f);

			friend struct UIContainer_TypeRegistrar;
			friend struct UIImage_TypeRegistrar;
			friend struct UIPanel_TypeRegistrar;
			friend struct UIText_TypeRegistrar;
		};
#pragma warning(pop)

	}
}
