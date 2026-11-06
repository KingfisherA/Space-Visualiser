#version 410
#extension GL_ARB_shading_language_420pack : require

// A quarter of the source's size (across and down): the average of the 4x4 source texels under
// each target texel, from four bilinear taps. Each tap is weighted down by its brightness (Karis
// 2014), so a single bright pixel doesn't flicker in the bloom as it moves.
layout(set = 0, binding = 0) uniform sampler2D uSource;

// Mirrors BloomPushConstants in src/SV/Graphics/BloomCompositor.cpp
layout(push_constant) uniform BloomPushConstants
{
    vec4 params; // xy = the source's texel size (UV), zw = 1 / the target's size
} uBloom;

layout(location = 0) out vec4 fragColour;

void main()
{
    vec2 uv = gl_FragCoord.xy * uBloom.params.zw;
    vec2 offset = uBloom.params.xy;
    vec3 sum = vec3(0.0);
    float weights = 0.0;
    for (int i = 0; i < 4; i++)
    {
        vec2 tap = vec2(i & 1, i >> 1) * 2.0 - 1.0;
        vec3 colour = texture(uSource, uv + tap * offset).rgb;
        float weight = 1.0 / (1.0 + dot(colour, vec3(0.2126, 0.7152, 0.0722)));
        sum += colour * weight;
        weights += weight;
    }
    fragColour = vec4(sum / weights, 1.0);
}
