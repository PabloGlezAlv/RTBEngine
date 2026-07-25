#version 430 core

in vec3 vTexCoords;

out vec4 FragColor;

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

uniform samplerCube uSkybox;
uniform bool uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeight;
uniform float uFogHeightFalloff;
uniform float uFogStart;
uniform float uFogEnd;

void main() {
    vec3 sky = texture(uSkybox, vTexCoords).rgb;

    if (uFogEnabled && uFogDensity > 0.0) {
        vec3 dir = normalize(vTexCoords);
        // Approximate far-sky fog along the view ray using height falloff.
        float heightFactor = exp(-uFogHeightFalloff * max(viewPos.y - uFogHeight, 0.0));
        float skyFog = 1.0 - exp(-uFogDensity * heightFactor * uFogEnd * 0.35);
        float horizonBoost = 1.0 - abs(dir.y);
        skyFog = clamp(skyFog * (0.55 + 0.45 * horizonBoost), 0.0, 1.0);
        sky = mix(sky, uFogColor, skyFog);
    }

    FragColor = vec4(sky, 1.0);
}
