#version 330 core

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aInstancePos;
layout(location = 3) in vec4 aInstanceColor;
layout(location = 4) in float aInstanceSize;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vUV;
out vec4 vColor;

void main()
{
    vec3 offset = (uCameraRight * aCorner.x + uCameraUp * aCorner.y) * aInstanceSize;
    vec3 worldPos = aInstancePos + offset;
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    // Slight bias toward the camera to reduce z-fighting with nearby opaque meshes.
    gl_Position.z -= 0.002 * gl_Position.w;
    vUV = aUV;
    vColor = aInstanceColor;
}
