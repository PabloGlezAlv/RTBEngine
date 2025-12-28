#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in ivec4 aBoneIndices;
layout(location = 4) in vec4 aBoneWeights;

out vec3 vColor;
out vec2 vTexCoords;
out vec3 vNormal;
out vec3 vFragPos;
out vec4 vFragPosLightSpace;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

// Skeletal animation uniforms
const int MAX_BONES = 100;
uniform mat4 uBoneTransforms[MAX_BONES];
uniform bool uHasAnimation;

void main() {
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);
    float totalWeight = 0.0;

    if (uHasAnimation) {
        for (int i = 0; i < 4; i++) {
            int boneIndex = aBoneIndices[i];
            float weight = aBoneWeights[i];

            if (weight > 0.0 && boneIndex >= 0 && boneIndex < MAX_BONES) {
                mat4 boneTransform = uBoneTransforms[boneIndex];
                totalPosition += boneTransform * vec4(aPosition, 1.0) * weight;
                totalNormal += mat3(boneTransform) * aNormal * weight;
                totalWeight += weight;
            }
        }

        // Fallback: if no bone weights, use original position
        if (totalWeight < 0.001) {
            totalPosition = vec4(aPosition, 1.0);
            totalNormal = aNormal;
        } else {
            // Normalize the normal after blending
            totalNormal = normalize(totalNormal);
        }
    } else {
        totalPosition = vec4(aPosition, 1.0);
        totalNormal = aNormal;
    }

    gl_Position = uProjection * uView * uModel * totalPosition;
    vTexCoords = aTexCoords;
    vFragPos = vec3(uModel * totalPosition);
    vNormal = mat3(transpose(inverse(uModel))) * totalNormal;
    vFragPosLightSpace = uLightSpaceMatrix * vec4(vFragPos, 1.0);
}
