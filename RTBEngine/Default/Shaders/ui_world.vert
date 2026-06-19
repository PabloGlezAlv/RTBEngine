#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoords;

out vec2 vTexCoords;

uniform mat4 uModel;

layout(std140, binding = 1) uniform CameraData {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
    float _cameraPad0;
    mat4 viewProjection;
    vec3 cameraRight;
    float _cameraPad1;
    vec3 cameraUp;
    float _cameraPad2;
};

void main() {
    vTexCoords = aTexCoords;
    gl_Position = projection * view * uModel * vec4(aPosition, 1.0);
}
