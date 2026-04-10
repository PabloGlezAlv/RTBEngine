#pragma once

#include "../Math/Vectors/Vector2.h"
#include "../Math/Vectors/Vector4.h"
#include "../RTBEngineAPI.h"

namespace RTBEngine {
    namespace UI {

        class RTB_API RectTransform {
        public:
            RectTransform();
            ~RectTransform() = default;

            // Dirty flag management
            void SetDirty();
            bool IsDirty() const { return isDirty; }
            void ClearDirty() { isDirty = false; }

            // Anchors (0,0 = bottom-left, 1,1 = top-right)
            void SetAnchorMin(const RTBEngine::Math::Vector2& anchor);
            void SetAnchorMax(const RTBEngine::Math::Vector2& anchor);
            void SetAnchor(const RTBEngine::Math::Vector2& anchor);
            void SetAnchor(float x, float y);

            RTBEngine::Math::Vector2 GetAnchorMin() const { return anchorMin; }
            RTBEngine::Math::Vector2 GetAnchorMax() const { return anchorMax; }

            // Pivot (0,0 = bottom-left, 1,1 = top-right, 0.5,0.5 = center)
            void SetPivot(const RTBEngine::Math::Vector2& pivot);
            void SetPivot(float x, float y);

            RTBEngine::Math::Vector2 GetPivot() const { return pivot; }

            // Position relative to anchor
            void SetAnchoredPosition(const RTBEngine::Math::Vector2& pos);
            void SetAnchoredPosition(float x, float y);

            RTBEngine::Math::Vector2 GetAnchoredPosition() const { return anchoredPosition; }

            // Size
            void SetSize(const RTBEngine::Math::Vector2& size);
            void SetSize(float width, float height);

            RTBEngine::Math::Vector2 GetSize() const { return sizeDelta; }

            // Rotation (degrees)
            void SetRotation(float degrees);
            float GetRotation() const { return rotation; }

            // Scale (local)
            void SetScale(const RTBEngine::Math::Vector2& scale);
            void SetScale(float x, float y);

            RTBEngine::Math::Vector2 GetScale() const { return scale; }

            // World/accumulated scale (lossyScale = parentScale * localScale)
            RTBEngine::Math::Vector2 GetLossyScale() const { return lossyScale; }

            // Calculate world transform with hierarchy-aware scale propagation
            void CalculateWorldTransform(const RTBEngine::Math::Vector2& parentWorldPos,
                const RTBEngine::Math::Vector2& parentWorldSize,
                const RTBEngine::Math::Vector2& parentLossyScale);

            // Layout values (pre-scale, anchor-relative)
            RTBEngine::Math::Vector2 GetLayoutPosition() const { return layoutPosition; }
            RTBEngine::Math::Vector2 GetLayoutSize() const { return layoutSize; }
            RTBEngine::Math::Vector4 GetLayoutRect() const { return RTBEngine::Math::Vector4(layoutPosition.x, layoutPosition.y, layoutSize.x, layoutSize.y); }

            // World values (final screen-space, including all parent scales)
            RTBEngine::Math::Vector2 GetWorldPosition() const { return worldPosition; }
            RTBEngine::Math::Vector2 GetWorldSize() const { return worldSize; }
            RTBEngine::Math::Vector4 GetWorldRect() const { return RTBEngine::Math::Vector4(worldPosition.x, worldPosition.y, worldSize.x, worldSize.y); }

            RTBEngine::Math::Vector2 anchorMin{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 anchorMax{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 pivot{ 0.5f, 0.5f };
            RTBEngine::Math::Vector2 anchoredPosition{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 sizeDelta{ 100.0f, 100.0f };
            RTBEngine::Math::Vector2 scale{ 1.0f, 1.0f };
            float rotation = 0.0f;

        private:
            // Layout values (calculated from anchors, before scale)
            RTBEngine::Math::Vector2 layoutPosition{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 layoutSize{ 0.0f, 0.0f };

            // World values (final, accumulated through hierarchy)
            RTBEngine::Math::Vector2 worldPosition{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 worldSize{ 0.0f, 0.0f };
            RTBEngine::Math::Vector2 lossyScale{ 1.0f, 1.0f };

            // Dirty flag - indicates world values need recalculation
            bool isDirty = true;
        };

    } 
} 
