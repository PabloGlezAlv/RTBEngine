#version 430 core

in vec3 vColor;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;

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

vec3 CalcDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

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
