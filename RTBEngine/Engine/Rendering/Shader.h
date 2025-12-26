#pragma once
#include <GL/glew.h>
#include <string>
#include <unordered_map>
#include "../Math/Math.h"

namespace RTBEngine {
    namespace Rendering {

        class Shader {
        public:
            Shader();
            ~Shader();

            Shader(const Shader&) = delete;
            Shader& operator=(const Shader&) = delete;

            bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
            bool LoadFromStrings(const std::string& vertexSource, const std::string& fragmentSource);

            void Bind() const;
            void Unbind() const;

            GLuint GetProgramID() const { return programID; }
            bool IsCompiled() const { return isCompiled; }

            void SetBool(const std::string& name, bool value);
            void SetInt(const std::string& name, int value);
            void SetFloat(const std::string& name, float value);
            void SetVector2(const std::string& name, const Math::Vector2& value);
            void SetVector3(const std::string& name, const Math::Vector3& value);
            void SetVector4(const std::string& name, const Math::Vector4& value);
            void SetMatrix4(const std::string& name, const Math::Matrix4& value);

        private:
            GLuint CompileShader(GLenum type, const std::string& source);
            bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);
            std::string ReadFile(const std::string& filePath);
            GLint GetUniformLocation(const std::string& name);

            GLuint programID;
            bool isCompiled;
            std::unordered_map<std::string, GLint> uniformCache;
        };

    }
}