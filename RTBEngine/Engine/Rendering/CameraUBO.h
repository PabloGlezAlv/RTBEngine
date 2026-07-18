#pragma once

#include "../RTBEngineAPI.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

        class Camera;

        static constexpr unsigned int kCameraUBOBindingPoint = RHI::kCameraUBOBinding;

#pragma warning(push)
#pragma warning(disable: 4251)
        class RTB_API CameraUBO {
        public:
            static CameraUBO& GetInstance();

            void Upload(Camera* camera);
            void Bind() const;

        private:
            CameraUBO();
            ~CameraUBO();

            CameraUBO(const CameraUBO&) = delete;
            CameraUBO& operator=(const CameraUBO&) = delete;

            RHI::GpuId buffer = RHI::kInvalidGpuId;
        };
#pragma warning(pop)

    }
}
