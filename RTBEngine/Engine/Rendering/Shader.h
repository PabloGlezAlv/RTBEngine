#pragma once
#include "../RTBEngineAPI.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "../Math/Math.h"
#include "RHI/RenderTypes.h"

namespace RTBEngine {
    namespace Rendering {

#pragma warning(push)
#pragma warning(disable: 4251)
        // Binding point for the per-animator bone matrices UBO (BoneData block).
        static constexpr unsigned int kBoneUBOBindingPoint = RHI::kBoneUBOBinding;

        class RTB_API Shader {
        public:
            static constexpr int MaxBoneTransforms = 100;

            Shader();
            ~Shader();

            Shader(const Shader&) = delete;
            Shader& operator=(const Shader&) = delete;

            bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
            bool LoadFromStrings(const std::string& vertexSource, const std::string& fragmentSource);

            void Bind() const;
            void Unbind() const;

            unsigned int GetProgramID() const { return programID; }
            bool IsCompiled() const { return isCompiled; }

            void SetBool(const std::string& name, bool value);
            void SetInt(const std::string& name, int value);
            void SetFloat(const std::string& name, float value);
            void SetVector2(const std::string& name, const Math::Vector2& value);
            void SetVector3(const std::string& name, const Math::Vector3& value);
            void SetVector4(const std::string& name, const Math::Vector4& value);
            void SetMatrix4(const std::string& name, const Math::Matrix4& value);

        private:
            std::string ReadFile(const std::string& filePath);
            int GetUniformLocation(const std::string& name);

            RHI::GpuId programID = RHI::kInvalidGpuId;
            bool isCompiled = false;
            std::unordered_map<std::string, int> uniformCache;
        };
#pragma warning(pop)

    }
}
