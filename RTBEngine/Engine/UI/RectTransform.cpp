#include "RectTransform.h"
#include <algorithm>
#include <cmath>

namespace RTBEngine {
    namespace UI {

        RectTransform::RectTransform() {
            // Default to dirty so initial calculation happens
            isDirty = true;
        }

        void RectTransform::SetDirty() {
            isDirty = true;
        }

        void RectTransform::SetAnchorMin(const RTBEngine::Math::Vector2& anchor) {
            anchorMin = anchor;
            SetDirty();
        }

        void RectTransform::SetAnchorMax(const RTBEngine::Math::Vector2& anchor) {
            anchorMax = anchor;
            SetDirty();
        }

        void RectTransform::SetAnchor(const RTBEngine::Math::Vector2& anchor) {
            anchorMin = anchor;
            anchorMax = anchor;
            SetDirty();
        }

        void RectTransform::SetAnchor(float x, float y) {
            SetAnchor(RTBEngine::Math::Vector2(x, y));
        }

        void RectTransform::SetPivot(const RTBEngine::Math::Vector2& p) {
            pivot = p;
            SetDirty();
        }

        void RectTransform::SetPivot(float x, float y) {
            SetPivot(RTBEngine::Math::Vector2(x, y));
        }

        void RectTransform::SetAnchoredPosition(const RTBEngine::Math::Vector2& pos) {
            anchoredPosition = pos;
            SetDirty();
        }

        void RectTransform::SetAnchoredPosition(float x, float y) {
            SetAnchoredPosition(RTBEngine::Math::Vector2(x, y));
        }

        void RectTransform::SetSize(const RTBEngine::Math::Vector2& size) {
            sizeDelta = size;
            SetDirty();
        }

        void RectTransform::SetSize(float width, float height) {
            SetSize(RTBEngine::Math::Vector2(width, height));
        }

        void RectTransform::SetRotation(float degrees) {
            rotation = degrees;
            SetDirty();
        }

        void RectTransform::SetScale(const RTBEngine::Math::Vector2& s) {
            scale = s;
            SetDirty();
        }

        void RectTransform::SetScale(float x, float y) {
            SetScale(RTBEngine::Math::Vector2(x, y));
        }

        void RectTransform::CalculateWorldTransform(const RTBEngine::Math::Vector2& parentWorldPos,
            const RTBEngine::Math::Vector2& parentWorldSize,
            const RTBEngine::Math::Vector2& parentLossyScale) {

            // Unity-convention mapping to screen Y-down space:
            //   anchor.y=1 -> top of parent    -> screen_y = parentPos.y
            //   anchor.y=0 -> bottom of parent -> screen_y = parentPos.y + parentSize.y
            //   screen_anchorY = parentPos.y + parentSize.y * (1 - anchor.y)
            //
            // anchoredPosition.y positive = up (Unity convention):
            //   screenPos.y = anchorY - anchoredPos.y - (height * (1 - pivot.y))

            // Calculate anchor positions in parent world space
            // parentWorldPos is top-left, parentWorldSize is the scaled size
            RTBEngine::Math::Vector2 anchorMinPos(
                parentWorldPos.x + parentWorldSize.x * anchorMin.x,
                parentWorldPos.y + parentWorldSize.y * (1.0f - anchorMin.y)
            );

            RTBEngine::Math::Vector2 anchorMaxPos(
                parentWorldPos.x + parentWorldSize.x * anchorMax.x,
                parentWorldPos.y + parentWorldSize.y * (1.0f - anchorMax.y)
            );

            RTBEngine::Math::Vector2 scaledAnchoredPosition(
                anchoredPosition.x * parentLossyScale.x,
                anchoredPosition.y * parentLossyScale.y
            );

            RTBEngine::Math::Vector2 scaledSizeDelta(
                sizeDelta.x * parentLossyScale.x,
                sizeDelta.y * parentLossyScale.y
            );

            // Calculate layout size and position (before applying local scale)
            if (anchorMin.x == anchorMax.x && anchorMin.y == anchorMax.y) {
                // Fixed anchor mode: sizeDelta is the absolute size
                layoutSize = scaledSizeDelta;
                layoutPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + scaledAnchoredPosition.x - (layoutSize.x * pivot.x),
                    anchorMinPos.y - scaledAnchoredPosition.y - (layoutSize.y * pivot.y)
                );
            }
            else {
                // Stretched mode: anchor band defines base size
                float anchorTop    = std::min(anchorMinPos.y, anchorMaxPos.y);
                float anchorBottom = std::max(anchorMinPos.y, anchorMaxPos.y);

                RTBEngine::Math::Vector2 anchorSize(
                    std::abs(anchorMaxPos.x - anchorMinPos.x),
                    anchorBottom - anchorTop
                );

                layoutSize = RTBEngine::Math::Vector2(
                    anchorSize.x + scaledSizeDelta.x,
                    anchorSize.y + scaledSizeDelta.y
                );

                layoutPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + scaledAnchoredPosition.x,
                    anchorTop      - scaledAnchoredPosition.y
                );
            }

            // Accumulate scale from parent: lossyScale = parentLossyScale * localScale
            lossyScale = RTBEngine::Math::Vector2(
                parentLossyScale.x * scale.x,
                parentLossyScale.y * scale.y
            );

            // Calculate final world size: layoutSize already includes parent scale, so only local scale is applied here
            worldSize = RTBEngine::Math::Vector2(
                layoutSize.x * scale.x,
                layoutSize.y * scale.y
            );

            worldPosition = RTBEngine::Math::Vector2(
                layoutPosition.x + (layoutSize.x - worldSize.x) * pivot.x,
                layoutPosition.y + (layoutSize.y - worldSize.y) * pivot.y
            );

            // Mark as clean (calculated)
            isDirty = false;
        }

    }
}
