#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;

layout(std140) uniform CameraData {
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

out vec4 vColor;

void main()
{
    gl_Position = viewProjection * vec4(aPosition, 1.0);
    vColor = aColor;
}
