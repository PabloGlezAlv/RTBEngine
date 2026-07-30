#version 330 core

in vec2 vUV;

uniform sampler2D uInput;
uniform float uTexelSizeX;
uniform float uTexelSizeY;

out vec4 FragColor;

void main()
{
    vec2 texel = vec2(uTexelSizeX, uTexelSizeY);
    vec3 result = vec3(0.0);
    result += texture(uInput, vUV + texel * -4.0).rgb * 0.05;
    result += texture(uInput, vUV + texel * -3.0).rgb * 0.09;
    result += texture(uInput, vUV + texel * -2.0).rgb * 0.12;
    result += texture(uInput, vUV + texel * -1.0).rgb * 0.15;
    result += texture(uInput, vUV).rgb * 0.18;
    result += texture(uInput, vUV + texel *  1.0).rgb * 0.15;
    result += texture(uInput, vUV + texel *  2.0).rgb * 0.12;
    result += texture(uInput, vUV + texel *  3.0).rgb * 0.09;
    result += texture(uInput, vUV + texel *  4.0).rgb * 0.05;
    FragColor = vec4(result, 1.0);
}
