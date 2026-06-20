#include "UIElement.h"
#include "../Scene/GameObject.h"
#include "../Scene/SceneManager.h"

namespace RTBEngine {
	namespace UI {

		UIElement::UIElement() {
		}

		UIElement::~UIElement() {
		}

		void UIElement::PropagateDirtyToChildren() {
			if (ECS::SceneManager::GetInstance().IsSceneUnloading()) {
				return;
			}

			ECS::GameObject* owner = GetOwner();
			if (!owner || owner->IsBeingDestroyed()) {
				return;
			}

			for (ECS::GameObject* child : owner->GetChildren()) {
				if (!child || child->IsBeingDestroyed()) {
					continue;
				}

				UIElement* childUI = child->GetComponent<UIElement>();
				if (childUI) {
					RectTransform* childRT = childUI->GetRectTransform();
					if (childRT->IsDirty()) continue;
					childRT->SetDirty();
					childUI->PropagateDirtyToChildren();
				}
			}
		}

		void UIElement::SetAnchorMin(const Math::Vector2& value) {
			rectTransform.SetAnchorMin(value);
			PropagateDirtyToChildren();
		}

		void UIElement::SetAnchorMax(const Math::Vector2& value) {
			rectTransform.SetAnchorMax(value);
			PropagateDirtyToChildren();
		}

		void UIElement::SetPivot(const Math::Vector2& value) {
			rectTransform.SetPivot(value);
			PropagateDirtyToChildren();
		}

		void UIElement::SetAnchoredPosition(const Math::Vector2& value) {
			rectTransform.SetAnchoredPosition(value);
			PropagateDirtyToChildren();
		}

		void UIElement::SetSizeDelta(const Math::Vector2& value) {
			rectTransform.SetSize(value);
			PropagateDirtyToChildren();
		}

		void UIElement::SetRotation(float degrees) {
			rectTransform.SetRotation(degrees);
			PropagateDirtyToChildren();
		}

		void UIElement::SetScale(const Math::Vector2& value) {
			rectTransform.SetScale(value);
			if (ECS::SceneManager::GetInstance().IsSceneUnloading()) {
				return;
			}
			PropagateDirtyToChildren();
		}

		void UIElement::OnAwake() {
			rectTransform.SetDirty();
		}

		void UIElement::OnParentChanged(ECS::GameObject* oldParent, ECS::GameObject* newParent) {
			(void)oldParent;
			(void)newParent;

			rectTransform.SetDirty();
			PropagateDirtyToChildren();
		}

	}
}
