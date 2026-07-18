#version 330 core

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aInstancePos;
layout(location = 3) in vec4 aInstanceColor;
layout(location = 4) in float aInstanceSize;
layout(location = 5) in float aInstanceFrame;

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

uniform bool uSheetEnabled;
uniform int uSheetColumns;
uniform int uSheetRows;
uniform int uSheetFrameCount;

out vec2 vUV;
out vec4 vColor;

void main()
{
    vec3 offset = (cameraRight * aCorner.x + cameraUp * aCorner.y) * aInstanceSize;
    vec3 worldPos = aInstancePos + offset;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_Position.z -= 0.002 * gl_Position.w;

    vec2 uv = aUV;
    if (uSheetEnabled) {
        int frame = int(aInstanceFrame) % max(uSheetFrameCount, 1);
        int col = frame % max(uSheetColumns, 1);
        int row = frame / max(uSheetColumns, 1);
        row = max(uSheetRows, 1) - 1 - row;
        vec2 sheetScale = vec2(1.0 / float(max(uSheetColumns, 1)), 1.0 / float(max(uSheetRows, 1)));
        vec2 sheetOffset = vec2(float(col), float(row)) * sheetScale;
        uv = aUV * sheetScale + sheetOffset;
    }

    vUV = uv;
    vColor = aInstanceColor;
}
