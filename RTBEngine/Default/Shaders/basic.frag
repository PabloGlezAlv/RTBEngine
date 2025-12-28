#version 430 core

in vec3 vColor;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

out vec4 FragColor;

// Texture and color
uniform sampler2D uTexture;
uniform bool uHasTexture;
uniform vec4 uColor;
uniform vec3 uDiffuseColor;
uniform vec3 uViewPos;

// Directional Light
struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};
uniform DirectionalLight dirLight;

// Point Lights
#define MAX_POINT_LIGHTS 8
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float range;
};
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

// Spot Lights
#define MAX_SPOT_LIGHTS 8
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
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform int numSpotLights;

uniform sampler2D uShadowMap;
uniform bool uHasShadows;
uniform float uShadowBias;

vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float ShadowCalculation(vec4 fragPosLightSpace, float bias);

void main() {
    vec3 norm = normalize(vNormal);
    vec3 viewDir = normalize(uViewPos - vFragPos);
    
    // Ambient
    vec3 ambient = 0.1 * dirLight.color;
    
    // Directional light
    vec3 result = CalcDirectionalLight(dirLight, norm, viewDir);
    
    // Point lights
    for (int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, vFragPos, viewDir);
    }
    
    // Spot lights
    for (int i = 0; i < numSpotLights && i < MAX_SPOT_LIGHTS; i++) {
        result += CalcSpotLight(spotLights[i], norm, vFragPos, viewDir);
    }
    
    result += ambient;
    
    if (uHasShadows) {
        float shadow = ShadowCalculation(vFragPosLightSpace, uShadowBias);
        result = ambient + (1.0 - shadow) * (result - ambient);
    }
    
    vec4 texColor = uHasTexture ? texture(uTexture, vTexCoords) : vec4(1.0);
    FragColor = vec4(result * uDiffuseColor, 1.0) * texColor * uColor;
}


vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
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

float ShadowCalculation(vec4 fragPosLightSpace, float bias) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    vec3 lightDir = normalize(-dirLight.direction);
    vec3 normal = normalize(vNormal);
    float NdotL = dot(normal, lightDir);

    // Adaptive bias: more bias for surfaces facing away, less for surfaces facing light
    float baseBias = 0.003;
    float slopeBias = baseBias * sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.1);
    slopeBias = clamp(slopeBias, 0.002, 0.03);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - slopeBias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

