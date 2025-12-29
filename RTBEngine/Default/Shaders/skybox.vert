#version 430 core

layout(location = 0) in vec3 aPosition;

out vec3 vTexCoords;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // Use position as texture coordinates for cubemap sampling
    vTexCoords = aPosition;
    
    // Transform vertex position
    vec4 pos = uProjection * uView * vec4(aPosition, 1.0);
    
    // Set z = w so depth is always 1.0 (maximum depth, rendered behind everything)
    gl_Position = pos.xyww;
}
