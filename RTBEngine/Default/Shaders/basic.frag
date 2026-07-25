#version 430 core

in vec3 vColor;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;
in vec4 vInstanceColor;

out vec4 FragColor;

// Texture and color
uniform sampler2D uTexture;
uniform bool uHasTexture;
uniform vec4 uColor;
uniform vec3 uDiffuseColor;
uniform bool uUseInstanceColor;

layout(std140, binding = 1) uniform CameraData {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
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

uniform sampler2D uShadowMap;
uniform bool uHasShadows;
uniform float uShadowBias;

uniform bool uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeight;
uniform float uFogHeightFalloff;
uniform float uFogStart;
uniform float uFogEnd;

// DDGI sampling inputs (CPU: DDGIVolume::UploadUBO + BindForSampling).
// Binding 7 = UBO params; 8/9 = mismos atlas que escribió ddgi_trace.comp.
layout(std140, binding = 7) uniform DDGIData {
    vec3 ddgiOrigin;           // volumen
    float _ddgiPad0;
    vec3 ddgiSpacing;          // extent/grid
    float _ddgiPad1;
    ivec3 ddgiGridDims;
    int ddgiEnabled;           // 0 → ambient; 1 → sample probes
    float ddgiHysteresis;
    float ddgiNormalBias;
    float ddgiViewBias;
    float ddgiProbeRadius;
    vec3 ddgiAmbientColor;     // fallback OFF
    float ddgiAmbientIntensity;
    float ddgiIntensity;       // escala del rebote ON
};

uniform sampler2D uDDGIIrradiance; // atlas color (sqrt-encoded)
uniform sampler2D uDDGIDistance;   // atlas mean / mean² (Chebyshev)
// Enable real: ddgiEnabled del UBO (no usar un bool suelto: Vulkan lo puede strippear).

vec2 DDGIOctEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z >= 0.0) n.xy = n.xy * 0.5 + 0.5;
    else n.xy = (1.0 - abs(n.yx)) * sign(n.xy) * 0.5 + 0.5;
    return n.xy;
}

// RTXGI-style irradiance query: trilinear + wrap shading + softened Chebyshev.
// Atlas tiles are (octSize+2)^2 with 1-texel borders; sample the interior.
vec3 SampleDDGI(vec3 worldPos, vec3 normal) {
    if (ddgiEnabled == 0) {
        return ddgiAmbientColor * ddgiAmbientIntensity;
    }

    vec3 N = normalize(normal);
    float nBias = max(ddgiNormalBias, 0.05);
    vec3 biasedPos = worldPos + N * nBias;

    vec3 rel = (biasedPos - ddgiOrigin) / ddgiSpacing;
    vec3 gridPos = rel - 0.5;
    ivec3 base = ivec3(floor(gridPos));
    vec3 alpha = clamp(fract(gridPos), vec3(0.0), vec3(1.0));

    vec3 irradiance = vec3(0.0);
    float totalWeight = 0.0;
    const int octSize = 16;
    const int probeTexels = octSize + 2;
    vec2 atlasSize = vec2(float(ddgiGridDims.x * probeTexels),
                          float(ddgiGridDims.z * ddgiGridDims.y * probeTexels));

    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
        ivec3 coord = clamp(base + offset, ivec3(0), ddgiGridDims - ivec3(1));

        vec3 probePos = ddgiOrigin + (vec3(coord) + 0.5) * ddgiSpacing;
        vec3 worldToProbe = probePos - worldPos;
        vec3 biasedToProbe = probePos - biasedPos;
        float biasedDist = length(biasedToProbe);
        vec3 biasedDir = biasedDist > 1e-4 ? biasedToProbe / biasedDist : N;
        vec3 worldDir = length(worldToProbe) > 1e-4 ? normalize(worldToProbe) : N;

        vec3 tri = mix(1.0 - alpha, alpha, vec3(offset));
        float trilinearWeight = max(tri.x * tri.y * tri.z, 0.001);

        // Backface: soft but no large constant floor (that looked like light through walls).
        float wrapShading = (dot(worldDir, N) + 1.0) * 0.5;
        float weight = wrapShading * wrapShading + 0.02;

        vec2 tileOrigin = vec2(float(coord.x * probeTexels),
                               float((coord.z * ddgiGridDims.y + coord.y) * probeTexels));

        // Chebyshev visibility — soften variance so near-floor probes are not crushed.
        vec2 distOct = DDGIOctEncode(-biasedDir);
        vec2 distLocal = clamp(distOct * float(octSize), vec2(0.5), vec2(float(octSize) - 0.5)) + 1.0;
        vec2 distUv = (tileOrigin + distLocal) / atlasSize;
        vec2 moments = texture(uDDGIDistance, distUv).rg;
        float mean = max(moments.x, 0.01);
        float mean2 = max(moments.y, mean * mean);
        float variance = max(abs(mean * mean - mean2), mean * 0.12);

        float chebyshevWeight = 1.0;
        if (biasedDist > mean) {
            float v = biasedDist - mean;
            chebyshevWeight = variance / (variance + v * v);
            chebyshevWeight = chebyshevWeight * chebyshevWeight; // square, not cube
        }
        weight *= max(0.08, chebyshevWeight);

        // Crush only extremely tiny weights.
        const float crushThreshold = 0.12;
        if (weight < crushThreshold) {
            weight *= weight / crushThreshold;
        }

        weight = max(weight, 1e-6);
        weight *= trilinearWeight;

        // Irradiance: sample normal, plus a slight lift toward +Y so floors pick up
        // wall/courtyard bounce stored in more horizontal probe directions.
        vec3 irrDir = normalize(N + vec3(0.0, 0.35, 0.0));
        vec2 irrOct = DDGIOctEncode(irrDir);
        vec2 irrLocal = clamp(irrOct * float(octSize), vec2(0.5), vec2(float(octSize) - 0.5)) + 1.0;
        vec2 irrUv = (tileOrigin + irrLocal) / atlasSize;
        vec3 probeIrr = texture(uDDGIIrradiance, irrUv).rgb;
        probeIrr *= probeIrr; // decode sqrt

        irradiance += probeIrr * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 1e-5) {
        return vec3(0.0);
    }
    irradiance /= totalWeight;
    // Hemisphere factor, then project intensity (bounce should stay below direct).
    irradiance *= 2.0 * 3.14159265;
    return clamp(irradiance * ddgiIntensity, vec3(0.0), vec3(4.0));
}

vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float ShadowCalculation(vec4 fragPosLightSpace, float bias);

float ComputeFogFactor(vec3 worldPos)
{
    if (!uFogEnabled || uFogDensity <= 0.0) {
        return 0.0;
    }

    float dist = length(worldPos - viewPos);
    float heightFactor = exp(-uFogHeightFalloff * max(worldPos.y - uFogHeight, 0.0));
    float exponential = 1.0 - exp(-uFogDensity * heightFactor * dist);
    float linear = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
    return clamp(max(exponential, linear * exponential), 0.0, 1.0);
}

void main() {
    vec3 norm = normalize(vNormal);
    vec3 viewDir = normalize(viewPos - vFragPos);

    // DDGI indirect diffuse (falls back to neutral ambient when disabled)
    vec3 gi = SampleDDGI(vFragPos, norm);

    // Directional light contribution
    vec3 dirLightContrib = CalcDirectionalLight(dirLight, norm, viewDir);

    // Point lights
    vec3 pointLightContrib = vec3(0.0);
    for (int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        pointLightContrib += CalcPointLight(pointLights[i], norm, vFragPos, viewDir);
    }

    // Spot lights
    vec3 spotLightContrib = vec3(0.0);
    for (int i = 0; i < numSpotLights && i < MAX_SPOT_LIGHTS; i++) {
        spotLightContrib += CalcSpotLight(spotLights[i], norm, vFragPos, viewDir);
    }

    // Apply shadow only to directional light (which casts shadows)
    float shadow = 0.0;
    if (uHasShadows) {
        shadow = ShadowCalculation(vFragPosLightSpace, uShadowBias);
    }

    // DDGI is low-frequency *indirect* irradiance (RTXGI). Keep direct shadowed lighting separate.
    vec3 lighting = gi + (1.0 - shadow) * dirLightContrib;

    vec3 result = lighting + pointLightContrib + spotLightContrib;

    vec4 texColor = uHasTexture ? texture(uTexture, vTexCoords) : vec4(1.0);
    if (uHasTexture && texColor.a < 0.01) {
        discard;
    }

    vec3 albedo = uDiffuseColor * texColor.rgb;
    vec3 litColor = result * albedo;
    vec4 effectiveColor = uUseInstanceColor ? vInstanceColor : uColor;
    vec3 tint = min(effectiveColor.rgb, vec3(1.0));
    vec3 flashAdd = max(effectiveColor.rgb - vec3(1.0), vec3(0.0));
    vec3 finalColor = litColor * tint + flashAdd;

    float fogFactor = ComputeFogFactor(vFragPos);
    finalColor = mix(finalColor, uFogColor, fogFactor);

    FragColor = vec4(finalColor, texColor.a * effectiveColor.a);
}


vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse (half-lambert softens the terminator so flat/stylized palette
    // textures shade smoothly instead of looking blotchy)
    float ndotl = dot(normal, lightDir);
    float diff = ndotl * 0.5 + 0.5;
    diff *= diff;
    vec3 diffuse = diff * light.color * light.intensity;
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * light.color * light.intensity * 0.5;
    
    return diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    
    // Skip if out of range
    if (distance > light.range) {
        return vec3(0.0);
    }
    
    // Attenuation
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color * light.intensity;
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * light.color * light.intensity * 0.5;
    
    return (diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float distance = length(light.position - fragPos);
    
    // Skip if out of range
    if (distance > light.range) {
        return vec3(0.0);
    }
    
    // Spotlight cone intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.innerCutOff - light.outerCutOff;
    float spotIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // If outside the cone, no light
    if (theta < light.outerCutOff) {
        return vec3(0.0);
    }
    
    // Attenuation
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color * light.intensity;
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * light.color * light.intensity * 0.5;
    
    return (diffuse + specular) * attenuation * spotIntensity;
}

vec2 poissonDisk[4] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760)
);

float ShadowCalculation(vec4 fragPosLightSpace, float bias) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform from [-1,1] to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Outside shadow map bounds = no shadow
    if (projCoords.z > 1.0)
        return 0.0;

    // Slope-based bias to prevent shadow acne
    vec3 lightDir = normalize(-dirLight.direction);
    vec3 normal = normalize(vNormal);
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float slopeBias = 0.005 * tan(acos(cosTheta));
    slopeBias = clamp(slopeBias, 0.0, 0.01);

    float currentDepth = projCoords.z - slopeBias;

    // Poisson disk sampling for soft shadows
    float shadow = 0.0;
    for (int i = 0; i < 4; i++) {
        float closestDepth = texture(uShadowMap, projCoords.xy + poissonDisk[i] / 700.0).r;
        if (currentDepth > closestDepth) {
            shadow += 0.2;
        }
    }

    return shadow;
}

