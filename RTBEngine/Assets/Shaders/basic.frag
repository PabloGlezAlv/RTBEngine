#version 430 core

in vec3 vColor;
in vec2 vTexCoords;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoords);
    FragColor = texColor * uColor;
}