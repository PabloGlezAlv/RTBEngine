#version 330 core

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aInstancePos;
layout(location = 3) in vec4 aInstanceColor;
layout(location = 4) in float aInstanceSize;

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

out vec2 vUV;
out vec4 vColor;

void main()
{
    vec3 offset = (cameraRight * aCorner.x + cameraUp * aCorner.y) * aInstanceSize;
    vec3 worldPos = aInstancePos + offset;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_Position.z -= 0.002 * gl_Position.w;
    vUV = aUV;
    vColor = aInstanceColor;
}
