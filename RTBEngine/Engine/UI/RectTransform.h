#pragma once

#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector4.h"

namespace RTBEngine {
    namespace UI {

        class RectTransform {
        public:
            RectTransform();
            ~RectTransform() = default;

            // Anchors (0,0 = bottom-left, 1,1 = top-right)
            void SetAnchorMin(const RTBEngine::Math::Vector2& anchor) { anchorMin = anchor; }
            void SetAnchorMax(const RTBEngine::Math::Vector2& anchor) { anchorMax = anchor; }
            void SetAnchor(const RTBEngine::Math::Vector2& anchor);
            void SetAnchor(float x, float y);

            RTBEngine::Math::Vector2 GetAnchorMin() const { return anchorMin; }
            RTBEngine::Math::Vector2 GetAnchorMax() const { return anchorMax; }

            // Pivot (0,0 = bottom-left, 1,1 = top-right, 0.5,0.5 = center)
            void SetPivot(const RTBEngine::Math::Vector2& pivot) { this->pivot = pivot; }
            void SetPivot(float x, float y);

            RTBEngine::Math::Vector2 GetPivot() const { return pivot; }

            // Position relative to anchor
            void SetAnchoredPosition(const RTBEngine::Math::Vector2& pos) { anchoredPosition = pos; }
            void SetAnchoredPosition(float x, float y);

            RTBEngine::Math::Vector2 GetAnchoredPosition() const { return anchoredPosition; }

            // Size
            void SetSize(const RTBEngine::Math::Vector2& size) { sizeDelta = size; }
            void SetSize(float width, float height);

            RTBEngine::Math::Vector2 GetSize() const { return sizeDelta; }

            // Rotation (degrees)
            void SetRotation(float degrees) { rotation = degrees; }
            float GetRotation() const { return rotation; }

            // Scale
            void SetScale(const RTBEngine::Math::Vector2& scale) { this->scale = scale; }
            void SetScale(float x, float y);

            RTBEngine::Math::Vector2 GetScale() const { return scale; }

            // Calculate final screen rect based on parent
            void CalculateScreenRect(const RTBEngine::Math::Vector2& parentPos,
                const RTBEngine::Math::Vector2& parentSize);

            RTBEngine::Math::Vector2 GetScreenPosition() const { return screenPosition; }
            RTBEngine::Math::Vector2 GetScreenSize() const { return screenSize; }
            RTBEngine::Math::Vector4 GetScreenRect() const { return RTBEngine::Math::Vector4(screenPosition.x, screenPosition.y, screenSize.x, screenSize.y); }

        private:
            RTBEngine::Math::Vector2 anchorMin{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 anchorMax{ 1.0f, 1.0f };
            RTBEngine::Math::Vector2 pivot{ 0.5f, 0.5f };
            RTBEngine::Math::Vector2 anchoredPosition{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 sizeDelta{ 100.0f, 100.0f };
            RTBEngine::Math::Vector2 scale{ 1.0f, 1.0f };

            float rotation = 0.0f;

            RTBEngine::Math::Vector2 screenPosition{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 screenSize{ 0.0f, 0.0f };
        };

    } 
} 
