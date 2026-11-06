#version 410
#extension GL_ARB_shading_language_420pack : require

// One direction of a separable Gaussian blur: nine texels from five bilinear taps
layout(set = 0, binding = 0) uniform sampler2D uSource;

// Mirrors BloomPushConstants in src/SV/Graphics/BloomCompositor.cpp
layout(push_constant) uniform BloomPushConstants
{
    vec4 params; // xy = the offset between texels along the blur (UV), zw = 1 / the target's size
} uBloom;

layout(location = 0) out vec4 fragColour;

const float OFFSETS[3] = float[](0.0, 1.3846153846, 3.2307692308);
const float WEIGHTS[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

void main()
{
    vec2 uv = gl_FragCoord.xy * uBloom.params.zw;
    vec3 sum = texture(uSource, uv).rgb * WEIGHTS[0];
    for (int i = 1; i < 3; i++)
    {
        vec2 offset = uBloom.params.xy * OFFSETS[i];
        sum += (texture(uSource, uv + offset).rgb + texture(uSource, uv - offset).rgb) * WEIGHTS[i];
    }
    fragColour = vec4(sum, 1.0);
}
