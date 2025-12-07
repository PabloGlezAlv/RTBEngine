#version 430 core

in vec3 vColor;
in vec2 vTexCoords;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoords);
}