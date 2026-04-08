#include "RectTransform.h"
#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace UI {

        RectTransform::RectTransform() {

        }

        void RectTransform::SetAnchor(const RTBEngine::Math::Vector2& anchor) {
            anchorMin = anchor;
            anchorMax = anchor;
        }

        void RectTransform::SetAnchor(float x, float y) {
            SetAnchor(RTBEngine::Math::Vector2(x, y));
        }

        void RectTransform::SetPivot(float x, float y) {
            pivot = RTBEngine::Math::Vector2(x, y);
        }

        void RectTransform::SetAnchoredPosition(float x, float y) {
            anchoredPosition = RTBEngine::Math::Vector2(x, y);
        }

        void RectTransform::SetSize(float width, float height) {
            sizeDelta = RTBEngine::Math::Vector2(width, height);
        }

        void RectTransform::SetScale(float x, float y) {
            scale = RTBEngine::Math::Vector2(x, y);
        }

        void RectTransform::CalculateScreenRect(const RTBEngine::Math::Vector2& parentPos,
            const RTBEngine::Math::Vector2& parentSize) {

            // Unity-convention mapping to screen Y-down space:
            //   anchor.y=1 -> top of parent    -> screen_y = parentPos.y
            //   anchor.y=0 -> bottom of parent -> screen_y = parentPos.y + parentSize.y
            //   screen_anchorY = parentPos.y + parentSize.y * (1 - anchor.y)
            //
            // anchoredPosition.y positive = up (Unity convention):
            //   screenPos.y = anchorY - anchoredPos.y - (height * (1 - pivot.y))
            //
            // pivot.y=1 -> top edge at anchor    -> offset = height*(1-1)=0  -> top-left is anchorY
            // pivot.y=0 -> bottom edge at anchor -> offset = height*(1-0)=h  -> top-left is anchorY - height
            // pivot.y=0.5 -> center at anchor    -> offset = height*0.5

            RTBEngine::Math::Vector2 anchorMinPos(
                parentPos.x + parentSize.x *  anchorMin.x,
                parentPos.y + parentSize.y * (1.0f - anchorMin.y)
            );

            RTBEngine::Math::Vector2 anchorMaxPos(
                parentPos.x + parentSize.x *  anchorMax.x,
                parentPos.y + parentSize.y * (1.0f - anchorMax.y)
            );

            if (anchorMin.x == anchorMax.x && anchorMin.y == anchorMax.y) {
                layoutSize = sizeDelta;
                layoutPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + anchoredPosition.x - (layoutSize.x * pivot.x),
                    anchorMinPos.y - anchoredPosition.y - (layoutSize.y * pivot.y)
                );

                screenSize = RTBEngine::Math::Vector2(
                    layoutSize.x * scale.x,
                    layoutSize.y * scale.y
                );

                screenPosition = RTBEngine::Math::Vector2(
                    layoutPosition.x + (layoutSize.x - screenSize.x) * pivot.x,
                    layoutPosition.y + (layoutSize.y - screenSize.y) * pivot.y
                );
            }
            else {
                // Stretched mode: anchor band defines base size.
                float anchorTop    = std::min(anchorMinPos.y, anchorMaxPos.y);
                float anchorBottom = std::max(anchorMinPos.y, anchorMaxPos.y);

                RTBEngine::Math::Vector2 anchorSize(
                    std::abs(anchorMaxPos.x - anchorMinPos.x),
                    anchorBottom - anchorTop
                );

                layoutSize = RTBEngine::Math::Vector2(
                    anchorSize.x + sizeDelta.x,
                    anchorSize.y + sizeDelta.y
                );

                layoutPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + anchoredPosition.x,
                    anchorTop      - anchoredPosition.y
                );

                screenSize = RTBEngine::Math::Vector2(
                    layoutSize.x * scale.x,
                    layoutSize.y * scale.y
                );

                screenPosition = layoutPosition;
            }
        }

    }
}
