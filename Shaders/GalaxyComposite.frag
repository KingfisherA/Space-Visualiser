#version 410
#extension GL_ARB_shading_language_420pack : require

// The galaxy's glow (at a lower resolution) and its stars (at the target's), both HDR radiance
layout(set = 1, binding = 0) uniform sampler2D uGlow;
layout(set = 1, binding = 1) uniform sampler2D uStars;

// Mirrors CompositePushConstants in src/SV/Universe/Galaxy.cpp
layout(push_constant) uniform CompositePushConstants
{
    vec4 params; // x = exposure, y = 1 to add the radiance (times the exposure) to an HDR target instead
} uComposite;

layout(location = 0) out vec4 fragColour;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(uStars, 0));
    vec3 radiance = texture(uGlow, uv).rgb + texelFetch(uStars, ivec2(gl_FragCoord.xy), 0).rgb;
    if (uComposite.params.y > 0.5)
        fragColour = vec4(max(radiance, vec3(0.0)) * uComposite.params.x, 0.0);
    else
        fragColour = vec4(pow(vec3(1.0) - exp(-max(radiance, vec3(0.0)) * uComposite.params.x), vec3(1.0 / 2.2)), 1.0);
}
