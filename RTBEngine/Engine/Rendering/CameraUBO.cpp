#include "CameraUBO.h"
#include "Camera.h"
#include "RHI/RenderDevice.h"

#include <cstdint>
#include <cstring>

namespace RTBEngine {
    namespace Rendering {

        namespace CameraUBOLayout {
            constexpr std::size_t kView = 0;
            constexpr std::size_t kProjection = 64;
            constexpr std::size_t kViewPos = 128;
            constexpr std::size_t kViewProjection = 144;
            constexpr std::size_t kCameraRight = 208;
            constexpr std::size_t kCameraUp = 224;
            constexpr std::size_t kBufferSize = 240;
        }

        namespace {
            void WriteMatrix4(std::uint8_t* buffer, const std::size_t offset, const Math::Matrix4& value)
            {
                std::memcpy(buffer + offset, value.m, sizeof(float) * 16);
            }

            void WriteVec3(std::uint8_t* buffer, const std::size_t offset, const Math::Vector3& value)
            {
                std::memcpy(buffer + offset, &value.x, sizeof(float) * 3);
            }

            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        CameraUBO& CameraUBO::GetInstance()
        {
            static CameraUBO instance;
            return instance;
        }

        CameraUBO::CameraUBO()
        {
            buffer = Device().CreateBuffer();
            Device().SetUniformBufferData(buffer, nullptr, CameraUBOLayout::kBufferSize, RHI::BufferUsage::Dynamic);
        }

        CameraUBO::~CameraUBO()
        {
            if (buffer != RHI::kInvalidGpuId && RHI::RenderDevice::HasDevice()) {
                Device().DestroyBuffer(buffer);
                buffer = RHI::kInvalidGpuId;
            }
        }

        void CameraUBO::Upload(Camera* camera)
        {
            std::uint8_t data[CameraUBOLayout::kBufferSize]{};

            if (camera) {
                WriteMatrix4(data, CameraUBOLayout::kView, camera->GetViewMatrix());
                WriteMatrix4(data, CameraUBOLayout::kProjection, camera->GetProjectionMatrix());
                WriteVec3(data, CameraUBOLayout::kViewPos, camera->GetPosition());
                WriteMatrix4(data, CameraUBOLayout::kViewProjection, camera->GetViewProjectionMatrix());
                WriteVec3(data, CameraUBOLayout::kCameraRight, camera->GetRight());
                WriteVec3(data, CameraUBOLayout::kCameraUp, camera->GetUp());
            }

            Device().UpdateUniformBufferData(buffer, data, CameraUBOLayout::kBufferSize);
        }

        void CameraUBO::Bind() const
        {
            Device().BindUniformBufferBase(buffer, kCameraUBOBindingPoint);
        }

    }
}
