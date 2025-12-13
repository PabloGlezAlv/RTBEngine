#version 430 core

in vec3 vColor;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

void main() {

    vec3 norm = normalize(vNormal);
    
    // Ambient light
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * uLightColor;
    
    // Diffuse light
    vec3 lightDir = normalize(-uLightDir); // Light direction is inverted to get normal with same dir
    float diff = max(dot(norm, lightDir), 0.0); //Calculate perpendicularity
    vec3 diffuse = diff * uLightColor;
    
    // Specular light
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm); 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32); 
    vec3 specular = specularStrength * spec * uLightColor;
    
    vec3 lighting = ambient + diffuse + specular;

    vec4 texColor = texture(uTexture, vTexCoords);

    FragColor = vec4(lighting, 1.0) * texColor * uColor;
}