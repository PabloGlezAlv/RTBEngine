#version 330 core

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aUV;

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

uniform vec3 uWorldPosition;
uniform vec2 uSize;
uniform float uVerticalOffset;
uniform vec4 uColor;
uniform bool uSheetEnabled;
uniform int uSheetColumns;
uniform int uSheetRows;
uniform int uSheetFrameCount;
uniform float uFrame;

out vec2 vUV;
out vec4 vColor;

void main()
{
    vec3 offset = (cameraRight * aCorner.x * uSize.x) + (cameraUp * aCorner.y * uSize.y);
    vec3 worldPos = uWorldPosition + cameraUp * uVerticalOffset + offset;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_Position.z -= 0.0015 * gl_Position.w;

    vec2 uv = aUV;
    if (uSheetEnabled) {
        int frame = int(uFrame) % max(uSheetFrameCount, 1);
        int col = frame % max(uSheetColumns, 1);
        int row = frame / max(uSheetColumns, 1);
        row = max(uSheetRows, 1) - 1 - row;
        vec2 sheetScale = vec2(1.0 / float(max(uSheetColumns, 1)), 1.0 / float(max(uSheetRows, 1)));
        vec2 sheetOffset = vec2(float(col), float(row)) * sheetScale;
        uv = aUV * sheetScale + sheetOffset;
    }

    vUV = uv;
    vColor = uColor;
}
