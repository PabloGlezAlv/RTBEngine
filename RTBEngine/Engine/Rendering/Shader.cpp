#include "Shader.h"
#include "RHI/RenderDevice.h"
#include <fstream>
#include <sstream>
#include "../Core/Logger.h"

namespace RTBEngine {
    namespace Rendering {

        namespace {
            void StripUtf8Bom(std::string& value)
            {
                if (value.size() >= 3
                    && static_cast<unsigned char>(value[0]) == 0xEF
                    && static_cast<unsigned char>(value[1]) == 0xBB
                    && static_cast<unsigned char>(value[2]) == 0xBF) {
                    value.erase(0, 3);
                }
            }

            RHI::IRenderDevice& Device()
            {
                return RHI::RenderDevice::Get();
            }
        }

        Shader::Shader() = default;

        Shader::~Shader() {
            if (programID != RHI::kInvalidGpuId) {
                if (RHI::RenderDevice::HasDevice()) {
                    Device().DestroyShaderProgram(programID);
                }
                programID = RHI::kInvalidGpuId;
            }
        }

        bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
            std::string vertexSource = ReadFile(vertexPath);
            std::string fragmentSource = ReadFile(fragmentPath);

            if (vertexSource.empty() || fragmentSource.empty()) {
                return false;
            }

            return LoadFromStrings(vertexSource, fragmentSource);
        }

        bool Shader::LoadFromStrings(const std::string& vertexSource, const std::string& fragmentSource) {
            if (programID != RHI::kInvalidGpuId) {
                Device().DestroyShaderProgram(programID);
                programID = RHI::kInvalidGpuId;
            }

            programID = Device().CreateShaderProgram(vertexSource, fragmentSource);
            isCompiled = (programID != RHI::kInvalidGpuId);
            if (isCompiled) {
                uniformCache.clear();
            }
            return isCompiled;
        }

        void Shader::Bind() const {
            Device().BindShaderProgram(programID);
        }

        void Shader::Unbind() const {
            Device().BindShaderProgram(RHI::kInvalidGpuId);
        }

        void Shader::SetBool(const std::string& name, bool value)
        {
            Device().SetUniformBool(GetUniformLocation(name), value);
        }

        void Shader::SetInt(const std::string& name, int value) {
            Device().SetUniformInt(GetUniformLocation(name), value);
        }

        void Shader::SetFloat(const std::string& name, float value) {
            Device().SetUniformFloat(GetUniformLocation(name), value);
        }

        void Shader::SetVector2(const std::string& name, const Math::Vector2& value) {
            Device().SetUniformVec2(GetUniformLocation(name), value.x, value.y);
        }

        void Shader::SetVector3(const std::string& name, const Math::Vector3& value) {
            Device().SetUniformVec3(GetUniformLocation(name), value.x, value.y, value.z);
        }

        void Shader::SetVector4(const std::string& name, const Math::Vector4& value) {
            Device().SetUniformVec4(GetUniformLocation(name), value.x, value.y, value.z, value.w);
        }

        void Shader::SetMatrix4(const std::string& name, const Math::Matrix4& value) {
            Device().SetUniformMat4(GetUniformLocation(name), value.m);
        }

        std::string Shader::ReadFile(const std::string& filePath) {
            std::ifstream file(filePath);

            if (!file.is_open()) {
                RTB_ERROR("Error: Could not open file: " + filePath);
                return "";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string contents = buffer.str();
            StripUtf8Bom(contents);
            return contents;
        }

        int Shader::GetUniformLocation(const std::string& name) {
            auto it = uniformCache.find(name);
            if (it != uniformCache.end()) {
                return it->second;
            }

            const int location = Device().GetUniformLocation(programID, name.c_str());
            uniformCache[name] = location;
            return location;
        }

    }
}
