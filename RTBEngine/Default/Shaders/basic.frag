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

    // Ambient - use a neutral gray, not tinted by light color
    vec3 ambient = vec3(0.1);

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

    // Combine lighting: ambient + shadowed directional + unshadowed point/spot
    vec3 result = ambient + (1.0 - shadow) * dirLightContrib + pointLightContrib + spotLightContrib;

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

