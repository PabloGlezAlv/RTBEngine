#include "UILayoutGroup.h"

#include "../../Scene/GameObject.h"
#include "../UIElement.h"

namespace RTBEngine {
    namespace UI {

        void UILayoutGroup::ApplyAxisLayout(bool horizontal) const
        {
            if (!owner) {
                return;
            }

            float cursorX = padding.x;
            float cursorY = -padding.y;

            for (Scene::GameObject* child : owner->GetChildren()) {
                if (!child || !child->IsActiveInHierarchy()) {
                    continue;
                }

                UIElement* uiElement = child->GetComponent<UIElement>();
                if (!uiElement || !uiElement->IsVisible()) {
                    continue;
                }

                const RTBEngine::Math::Vector2 topLeftAnchor(0.0f, 1.0f);
                uiElement->SetAnchorMin(topLeftAnchor);
                uiElement->SetAnchorMax(topLeftAnchor);
                uiElement->SetPivot(topLeftAnchor);

                const RTBEngine::Math::Vector2 childSize = uiElement->GetSizeDelta();
                if (horizontal) {
                    uiElement->SetAnchoredPosition(RTBEngine::Math::Vector2(cursorX, -padding.y));
                    cursorX += childSize.x + spacing;
                } else {
                    uiElement->SetAnchoredPosition(RTBEngine::Math::Vector2(padding.x, cursorY));
                    cursorY -= childSize.y + spacing;
                }
            }
        }

    }
}
