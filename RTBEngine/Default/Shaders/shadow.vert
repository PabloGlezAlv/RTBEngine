#version 430 core

layout (location = 0) in vec3 aPosition;
layout (location = 3) in ivec4 aBoneIndices;
layout (location = 4) in vec4 aBoneWeights;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

uniform bool uHasAnimation;
uniform mat4 uBoneTransforms[100];

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

    gl_Position = uLightSpaceMatrix * uModel * position;
}
