#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in ivec4 aBoneIndices;
layout(location = 4) in vec4 aBoneWeights;

layout(location = 5) in vec4 aInstanceMatrix0;
layout(location = 6) in vec4 aInstanceMatrix1;
layout(location = 7) in vec4 aInstanceMatrix2;
layout(location = 8) in vec4 aInstanceMatrix3;
layout(location = 9) in vec4 aInstanceColor;

out vec3 vColor;
out vec2 vTexCoords;
out vec3 vNormal;
out vec3 vFragPos;
out vec4 vFragPosLightSpace;
out vec4 vInstanceColor;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform bool uUseInstancing;
uniform bool uUseInstanceColor;

layout(std140, binding = 1) uniform CameraData {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
};

// Skeletal animation: bone matrices supplied via UBO, uploaded once per animator.
const int MAX_BONES = 100;
layout(std140, binding = 2) uniform BoneData {
    mat4 uBoneTransforms[MAX_BONES];
};
uniform bool uHasAnimation;

mat4 GetModelMatrix()
{
    if (uUseInstancing) {
        return mat4(
            aInstanceMatrix0,
            aInstanceMatrix1,
            aInstanceMatrix2,
            aInstanceMatrix3);
    }
    return uModel;
}

void main() {
    mat4 modelMatrix = GetModelMatrix();

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

    gl_Position = projection * view * modelMatrix * totalPosition;
    vTexCoords = aTexCoords;
    vFragPos = vec3(modelMatrix * totalPosition);
    vNormal = mat3(transpose(inverse(modelMatrix))) * totalNormal;
    vFragPosLightSpace = uLightSpaceMatrix * vec4(vFragPos, 1.0);
    vInstanceColor = uUseInstanceColor ? aInstanceColor : vec4(1.0);
}
