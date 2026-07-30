#version 330 core

in vec2 vUV;

uniform sampler2D uSceneColor;
uniform sampler2D uBloom;
uniform float uIntensity;

out vec4 FragColor;

vec3 ReinhardTonemap(vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

void main()
{
    vec3 scene = texture(uSceneColor, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 combined = scene + bloom * uIntensity;
    FragColor = vec4(ReinhardTonemap(combined), 1.0);
}
