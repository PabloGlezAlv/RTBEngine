#include "RectTransform.h"

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
            
            // Calculate anchor positions in parent space
            RTBEngine::Math::Vector2 anchorMinPos(
                parentPos.x + parentSize.x * anchorMin.x,
                parentPos.y + parentSize.y * anchorMin.y
            );
            
            RTBEngine::Math::Vector2 anchorMaxPos(
                parentPos.x + parentSize.x * anchorMax.x,
                parentPos.y + parentSize.y * anchorMax.y
            );

            // If anchors are the same, use sizeDelta as absolute size
            if (anchorMin.x == anchorMax.x && anchorMin.y == anchorMax.y) {
                screenSize = RTBEngine::Math::Vector2(
                    sizeDelta.x * scale.x,
                    sizeDelta.y * scale.y
                );
                
                // Position based on anchor point and pivot
                screenPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + anchoredPosition.x - (screenSize.x * pivot.x),
                    anchorMinPos.y + anchoredPosition.y - (screenSize.y * pivot.y)
                );
            }
            else {
                // Stretched mode: size is determined by anchors, sizeDelta is offset
                RTBEngine::Math::Vector2 anchorSize(
                    anchorMaxPos.x - anchorMinPos.x,
                    anchorMaxPos.y - anchorMinPos.y
                );
                
                screenSize = RTBEngine::Math::Vector2(
                    (anchorSize.x + sizeDelta.x) * scale.x,
                    (anchorSize.y + sizeDelta.y) * scale.y
                );
                
                // Position at anchor min, adjusted by pivot and anchored position
                screenPosition = RTBEngine::Math::Vector2(
                    anchorMinPos.x + anchoredPosition.x - (sizeDelta.x * 0.5f * pivot.x),
                    anchorMinPos.y + anchoredPosition.y - (sizeDelta.y * 0.5f * pivot.y)
                );
            }
        }

    } 
} 
