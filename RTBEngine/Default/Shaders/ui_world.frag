#version 430 core

in vec2 vTexCoords;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uHasTexture;
uniform vec4 uColor;

void main() {
    vec4 baseColor = uHasTexture ? texture(uTexture, vTexCoords) : vec4(1.0);
    vec4 finalColor = baseColor * uColor;

    if (finalColor.a <= 0.001) {
        discard;
    }

    FragColor = finalColor;
}
