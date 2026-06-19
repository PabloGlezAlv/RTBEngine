#include "CameraUBO.h"

#include "Camera.h"

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
        }

        CameraUBO& CameraUBO::GetInstance()
        {
            static CameraUBO instance;
            return instance;
        }

        CameraUBO::CameraUBO()
        {
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_UNIFORM_BUFFER, buffer);
            glBufferData(GL_UNIFORM_BUFFER, CameraUBOLayout::kBufferSize, nullptr, GL_DYNAMIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        CameraUBO::~CameraUBO()
        {
            if (buffer != 0) {
                glDeleteBuffers(1, &buffer);
                buffer = 0;
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

            glBindBuffer(GL_UNIFORM_BUFFER, buffer);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, CameraUBOLayout::kBufferSize, data);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }

        void CameraUBO::Bind() const
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, kCameraUBOBindingPoint, buffer);
        }

    }
}
