#version 430 core

layout(location = 0) in vec3 aPosition;

out vec3 vTexCoords;

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
    vTexCoords = aPosition;

    mat4 skyView = view;
    skyView[3] = vec4(0.0, 0.0, 0.0, 1.0);

    vec4 pos = projection * skyView * vec4(aPosition, 1.0);
    gl_Position = pos.xyww;
}
