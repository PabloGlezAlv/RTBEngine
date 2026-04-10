#pragma once
#include "../RTBEngineAPI.h"
#include "../ECS/Component.h"
#include "RectTransform.h"
#include "../Math/Vectors/Vector2.h"

namespace RTBEngine {
	namespace UI {

#pragma warning(push)
#pragma warning(disable: 4251)
		class RTB_API UIElement : public ECS::Component {
		public:
			UIElement();
			virtual ~UIElement();

			RectTransform* GetRectTransform() { return &rectTransform; }
			const RectTransform* GetRectTransform() const { return &rectTransform; }

			void PropagateDirtyToChildren();

			void SetVisible(bool visible) { isVisible = visible; }
			bool IsVisible() const { return isVisible; }

			void SetRaycastTarget(bool value) { raycastTarget = value; }
			bool IsRaycastTarget() const { return raycastTarget; }

			void SetAnchorMin(const Math::Vector2& value);
			void SetAnchorMax(const Math::Vector2& value);
			void SetPivot(const Math::Vector2& value);
			void SetAnchoredPosition(const Math::Vector2& value);
			void SetSizeDelta(const Math::Vector2& value);
			void SetRotation(float degrees);
			void SetScale(const Math::Vector2& value);

			Math::Vector2 GetAnchorMin() const { return rectTransform.GetAnchorMin(); }
			Math::Vector2 GetAnchorMax() const { return rectTransform.GetAnchorMax(); }
			Math::Vector2 GetPivot() const { return rectTransform.GetPivot(); }
			Math::Vector2 GetAnchoredPosition() const { return rectTransform.GetAnchoredPosition(); }
			Math::Vector2 GetSizeDelta() const { return rectTransform.GetSize(); }
			float GetRotation() const { return rectTransform.GetRotation(); }
			Math::Vector2 GetScale() const { return rectTransform.GetScale(); }

			virtual void OnAwake() override;
			virtual void OnParentChanged(ECS::GameObject* oldParent, ECS::GameObject* newParent) override;

			virtual const char* GetTypeName() const override = 0;

			virtual void Render() = 0;

			// Reflected properties that are intentionally editable at runtime.
			bool isVisible = true;
			bool raycastTarget = true;

		protected:
			RectTransform rectTransform;

		private:
			friend struct UIContainer_TypeRegistrar;
			friend struct UIImage_TypeRegistrar;
			friend struct UIPanel_TypeRegistrar;
			friend struct UIText_TypeRegistrar;
		};
#pragma warning(pop)

	}
}
