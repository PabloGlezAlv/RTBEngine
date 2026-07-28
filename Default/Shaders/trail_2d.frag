#version 330 core

in vec4 vColor;
in vec2 vUV;
in float vSide;

uniform float uSoftEdge;
uniform int uHasTexture;
uniform sampler2D uDiffuse;

out vec4 FragColor;

void main()
{
    float edgeAlpha = 1.0;
    if (uSoftEdge > 0.0001) {
        float soft = max(uSoftEdge, 0.0001);
        edgeAlpha = pow(max(1.0 - abs(vSide), 0.0), soft);
    }

    vec4 texColor = vec4(1.0);
    if (uHasTexture != 0) {
        texColor = texture(uDiffuse, vUV);
    }

    vec4 color = vColor * texColor;
    color.a *= edgeAlpha;
    FragColor = color;
}
