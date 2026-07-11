#version 430 core

layout (location = 0) in vec3 aPosition;
layout (location = 3) in ivec4 aBoneIndices;
layout (location = 4) in vec4 aBoneWeights;

layout (location = 5) in vec4 aInstanceMatrix0;
layout (location = 6) in vec4 aInstanceMatrix1;
layout (location = 7) in vec4 aInstanceMatrix2;
layout (location = 8) in vec4 aInstanceMatrix3;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform bool uUseInstancing;

uniform bool uHasAnimation;
const int MAX_BONES = 100;
layout(std140, binding = 2) uniform BoneData {
    mat4 uBoneTransforms[MAX_BONES];
};

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

void main()
{
    vec4 position = vec4(aPosition, 1.0);

    if (uHasAnimation) {
        mat4 boneTransform = mat4(0.0);
        for (int i = 0; i < 4; i++) {
            if (aBoneIndices[i] >= 0) {
                boneTransform += uBoneTransforms[aBoneIndices[i]] * aBoneWeights[i];
            }
        }
        position = boneTransform * position;
    }

    gl_Position = uLightSpaceMatrix * GetModelMatrix() * position;
}
