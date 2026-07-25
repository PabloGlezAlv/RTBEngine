#version 430 core

in vec2 vUV;

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

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float range;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float innerCutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    float range;
};

layout(std140, binding = 0) uniform LightingData {
    DirectionalLight dirLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    int numPointLights;
    int numSpotLights;
};

uniform sampler2D uSceneDepth;
uniform sampler2D uShadowMap;
uniform bool uHasShadows;
uniform float uShadowBias;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uViewProjection; // inverse view-projection

uniform bool uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeight;
uniform float uFogHeightFalloff;
uniform float uFogStart;
uniform float uFogEnd;
uniform bool uVolumetricFogEnabled;
uniform float uVolumetricIntensity;
uniform float uVolumetricAnisotropy;
uniform int uVolumetricSamples;
uniform float uCameraNear;
uniform float uCameraFar;
uniform bool uDepthZeroToOne;
uniform float uVolumetricMaxLuminance;

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4);
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

// Matches basic.frag + Vulkan inject (XY remap; Z already [0,1] when clip-corrected).
float SampleShadow(vec3 worldPos)
{
    if (!uHasShadows) {
        return 1.0;
    }

    vec4 lightSpace = uLightSpaceMatrix * vec4(worldPos, 1.0);
    if (abs(lightSpace.w) < 1e-6) {
        return 1.0;
    }
    vec3 proj = lightSpace.xyz / lightSpace.w;

    proj.xy = proj.xy * 0.5 + 0.5;
    if (!uDepthZeroToOne) {
        proj.z = proj.z * 0.5 + 0.5;
    }

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 1.0;
    }
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }

    float bias = max(uShadowBias, 0.003);
    float closest = texture(uShadowMap, proj.xy).r;
    return (proj.z - bias) > closest ? 0.0 : 1.0;
}

float HeightDensity(float worldY)
{
    return exp(-uFogHeightFalloff * max(worldY - uFogHeight, 0.0));
}

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    float zNdc = uDepthZeroToOne ? depth : (depth * 2.0 - 1.0);
    vec4 clip = vec4(uv * 2.0 - 1.0, zNdc, 1.0);
    vec4 world = uViewProjection * clip;
    return world.xyz / max(world.w, 1e-6);
}

void main()
{
    if (!uFogEnabled || !uVolumetricFogEnabled || uVolumetricIntensity <= 1e-5) {
        FragColor = vec4(0.0);
        return;
    }

    float depth = texture(uSceneDepth, vUV).r;
    vec3 worldEnd = ReconstructWorldPos(vUV, depth);
    vec3 rayOrigin = viewPos;
    vec3 ray = worldEnd - rayOrigin;
    float rayLength = length(ray);

    if (rayLength < 1e-3) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 rayDir = ray / rayLength;
    float maxDist = min(rayLength, max(uFogEnd, uCameraNear + 1.0));
    float marchStart = max(uCameraNear, 0.05);
    if (maxDist <= marchStart) {
        FragColor = vec4(0.0);
        return;
    }

    int samples = clamp(uVolumetricSamples, 8, 64);
    float marchLen = maxDist - marchStart;
    float stepSize = marchLen / float(samples);

    vec3 lightDir = normalize(-dirLight.direction);
    // Floor directional strength so indoor shafts remain readable with weak sun intensity.
    float lightStrength = max(dirLight.intensity, 0.75);
    vec3 lightColor = dirLight.color * lightStrength;
    vec3 inscatter = vec3(0.0);
    float transmittance = 1.0;

    float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    const float kShaftGain = 36.0;

    for (int i = 0; i < 64; ++i) {
        if (i >= samples) {
            break;
        }

        float t = marchStart + (float(i) + dither) * stepSize;
        if (t > maxDist) {
            break;
        }

        vec3 samplePos = rayOrigin + rayDir * t;
        float density = uFogDensity * HeightDensity(samplePos.y);
        if (density < 1e-7) {
            continue;
        }

        float shadow = SampleShadow(samplePos);
        float cosTheta = dot(rayDir, lightDir);
        float phase = max(HenyeyGreenstein(cosTheta, clamp(uVolumetricAnisotropy, -0.95, 0.95)), 0.35);

        vec3 lighting = lightColor * phase * shadow * kShaftGain + uFogColor * 0.015;
        float stepOptical = density * stepSize;
        inscatter += transmittance * lighting * density * stepSize;
        transmittance *= exp(-stepOptical);

        if (transmittance < 0.02) {
            break;
        }
    }

    inscatter *= uVolumetricIntensity;

    // Soft luminance ceiling (scaled Reinhard) for additive volumetric.
    float maxL = max(uVolumetricMaxLuminance, 0.05);
    float lum = dot(inscatter, vec3(0.2126, 0.7152, 0.0722));
    if (lum > 1e-5) {
        float compressed = (lum * maxL) / (lum + maxL);
        inscatter *= compressed / lum;
    }

    FragColor = vec4(max(inscatter, vec3(0.0)), 1.0);
}
