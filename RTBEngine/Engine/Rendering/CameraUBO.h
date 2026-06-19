#pragma once

#include "../RTBEngineAPI.h"
#include <GL/glew.h>

namespace RTBEngine {
    namespace Rendering {

        class Camera;

        static constexpr GLuint kCameraUBOBindingPoint = 1;

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

            GLuint buffer = 0;
        };
#pragma warning(pop)

    }
}
