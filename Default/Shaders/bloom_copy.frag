#version 330 core

in vec2 vUV;

uniform sampler2D uSource;

out vec4 FragColor;

void main()
{
    FragColor = texture(uSource, vUV);
}
