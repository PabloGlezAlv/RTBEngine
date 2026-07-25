#version 330 core

in vec2 vUV;
in vec4 vColor;
in vec3 vWorldPos;

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

uniform sampler2D uDiffuse;
uniform bool uHasTexture;
uniform bool uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeight;
uniform float uFogHeightFalloff;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 FragColor;

void main()
{
    vec4 texColor = uHasTexture ? texture(uDiffuse, vUV) : vec4(1.0);
    vec4 color = texColor * vColor;
    if (color.a < 0.15) {
        discard;
    }

    if (uFogEnabled && uFogDensity > 0.0) {
        float dist = length(vWorldPos - viewPos);
        float heightFactor = exp(-uFogHeightFalloff * max(vWorldPos.y - uFogHeight, 0.0));
        float fogFactor = 1.0 - exp(-uFogDensity * heightFactor * dist);
        float linear = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
        fogFactor = clamp(max(fogFactor, fogFactor * linear), 0.0, 1.0);
        color.rgb = mix(color.rgb, uFogColor, fogFactor);
    }

    FragColor = color;
}
