#include "Shader.h"
#include "Lighting/LightingUBO.h"
#include "CameraUBO.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "../RTBEngine.h"

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
        }

        Shader::Shader()
            : programID(0)
            , isCompiled(false)
        {
        }

        Shader::~Shader() {
            if (programID != 0) {
                glDeleteProgram(programID);
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
            GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
            if (vertexShader == 0) {
                return false;
            }

            GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
            if (fragmentShader == 0) {
                glDeleteShader(vertexShader);
                return false;
            }

            bool success = LinkProgram(vertexShader, fragmentShader);

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            isCompiled = success;
            return success;
        }

        void Shader::Bind() const {
            glUseProgram(programID);
        }

        void Shader::Unbind() const {
            glUseProgram(0);
        }

        void Shader::SetBool(const std::string& name, bool value)
        {
            glUniform1i(GetUniformLocation(name), value);
        }

        void Shader::SetInt(const std::string& name, int value) {
            glUniform1i(GetUniformLocation(name), value);
        }

        void Shader::SetFloat(const std::string& name, float value) {
            glUniform1f(GetUniformLocation(name), value);
        }

        void Shader::SetVector2(const std::string& name, const Math::Vector2& value) {
            glUniform2f(GetUniformLocation(name), value.x, value.y);
        }

        void Shader::SetVector3(const std::string& name, const Math::Vector3& value) {
            glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
        }

        void Shader::SetVector4(const std::string& name, const Math::Vector4& value) {
            glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
        }

        void Shader::SetMatrix4(const std::string& name, const Math::Matrix4& value) {
            glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, value.m);
        }

        GLuint Shader::CompileShader(GLenum type, const std::string& source) {
            GLuint shader = glCreateShader(type);
            const char* src = source.c_str();
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (!success) {
                GLchar infoLog[512];
                glGetShaderInfoLog(shader, 512, nullptr, infoLog);
                RTB_ERROR("Error: Shader compilation failed: " + std::string(infoLog));
                glDeleteShader(shader);
                return 0;
            }

            return shader;
        }

        bool Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
            programID = glCreateProgram();
            glAttachShader(programID, vertexShader);
            glAttachShader(programID, fragmentShader);
            glLinkProgram(programID);

            GLint success;
            glGetProgramiv(programID, GL_LINK_STATUS, &success);

            if (!success) {
                GLchar infoLog[512];
                glGetProgramInfoLog(programID, 512, nullptr, infoLog);
                RTB_ERROR("Error: Shader linking failed: " + std::string(infoLog));
                glDeleteProgram(programID);
                programID = 0;
                return false;
            }

            uniformCache.clear();

            const GLuint lightingBlockIndex = glGetUniformBlockIndex(programID, "LightingData");
            if (lightingBlockIndex != GL_INVALID_INDEX) {
                glUniformBlockBinding(programID, lightingBlockIndex, kLightingUBOBindingPoint);
            }

            const GLuint cameraBlockIndex = glGetUniformBlockIndex(programID, "CameraData");
            if (cameraBlockIndex != GL_INVALID_INDEX) {
                glUniformBlockBinding(programID, cameraBlockIndex, kCameraUBOBindingPoint);
            }

            const GLuint boneBlockIndex = glGetUniformBlockIndex(programID, "BoneData");
            if (boneBlockIndex != GL_INVALID_INDEX) {
                glUniformBlockBinding(programID, boneBlockIndex, kBoneUBOBindingPoint);
            }

            return true;
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

        GLint Shader::GetUniformLocation(const std::string& name) {
            auto it = uniformCache.find(name);
            if (it != uniformCache.end()) {
                return it->second;
            }

            GLint location = glGetUniformLocation(programID, name.c_str());
            uniformCache[name] = location;
            return location;
        }

    }
}