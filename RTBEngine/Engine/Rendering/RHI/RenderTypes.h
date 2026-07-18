#pragma once

#include "../../RTBEngineAPI.h"
#include <cstdint>

namespace RTBEngine {
    namespace Rendering {
        namespace RHI {

            enum class ClearMask : std::uint32_t {
                None = 0,
                Color = 1 << 0,
                Depth = 1 << 1,
                ColorDepth = Color | Depth
            };

            inline ClearMask operator|(ClearMask a, ClearMask b)
            {
                return static_cast<ClearMask>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
            }

            inline ClearMask operator&(ClearMask a, ClearMask b)
            {
                return static_cast<ClearMask>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
            }

            enum class DepthFunc {
                Less,
                LEqual,
                Greater,
                GEqual,
                Always,
                Never,
                Equal,
                NotEqual
            };

            enum class BufferUsage {
                Static,
                Dynamic
            };

            enum class PrimitiveTopology {
                Triangles,
                TriangleStrip,
                Lines,
                LineStrip,
                Points
            };

            enum class IndexType {
                UInt16,
                UInt32
            };

            enum class TextureFilter {
                Nearest,
                Linear,
                LinearMipmapLinear
            };

            enum class TextureWrap {
                Repeat,
                ClampToEdge,
                MirroredRepeat,
                ClampToBorder
            };

            enum class TextureFormat {
                R8,
                RGB8,
                RGBA8,
                SRGB8,
                SRGBA8,
                Depth24,
                Depth32F
            };

            // Blend factors match OpenGL numeric values so backends can pass them through.
            // OpenGL: GL_ZERO=0, GL_ONE=1, GL_SRC_ALPHA=0x0302, GL_ONE_MINUS_SRC_ALPHA=0x0303
            namespace BlendFactor {
                constexpr int Zero = 0;
                constexpr int One = 1;
                constexpr int SrcAlpha = 0x0302;
                constexpr int OneMinusSrcAlpha = 0x0303;
                constexpr int SrcColor = 0x0300;
                constexpr int OneMinusSrcColor = 0x0301;
                constexpr int DstAlpha = 0x0304;
                constexpr int OneMinusDstAlpha = 0x0305;
            }

            // Opaque GPU object ids for Phase 1 (OpenGL maps 1:1 to GLuint).
            using GpuId = unsigned int;

            static constexpr GpuId kInvalidGpuId = 0;

            // Standard UBO binding points (must match shaders).
            static constexpr unsigned int kLightingUBOBinding = 0;
            static constexpr unsigned int kCameraUBOBinding = 1;
            static constexpr unsigned int kBoneUBOBinding = 2;

        }
    }
}
