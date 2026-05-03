#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoords;

out vec2 vTexCoords;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vTexCoords = aTexCoords;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
