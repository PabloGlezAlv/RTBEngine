#version 330 core

in vec2 vUV;

uniform sampler2D uSceneColor;
uniform float uThreshold;

out vec4 FragColor;

void main()
{
    vec3 color = texture(uSceneColor, vUV).rgb;
    vec3 bright = max(color - vec3(uThreshold), vec3(0.0));
    float luminance = dot(bright, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(bright * smoothstep(0.0, 0.75, luminance), 1.0);
}
