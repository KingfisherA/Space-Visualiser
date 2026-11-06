#version 410
#extension GL_ARB_shading_language_420pack : require

// A scene (HDR radiance, at up to the target's resolution) plus its bloom (wider at each level),
// tone mapped
layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(set = 0, binding = 1) uniform sampler2D uBloom0;
layout(set = 0, binding = 2) uniform sampler2D uBloom1;
layout(set = 0, binding = 3) uniform sampler2D uBloom2;

// Mirrors CompositePushConstants in src/SV/Graphics/BloomCompositor.cpp
layout(push_constant) uniform CompositePushConstants
{
    vec4 params; // x = exposure, y = bloom, zw = 1 / the target's size
    vec4 blend;  // x = alpha (to blend over the target), y = colour scale (to add to it)
} uComposite;

layout(location = 0) out vec4 fragColour;

void main()
{
    vec2 uv = gl_FragCoord.xy * uComposite.params.zw;
    vec3 bloom = 0.5 * texture(uBloom0, uv).rgb + 0.3 * texture(uBloom1, uv).rgb + 0.2 * texture(uBloom2, uv).rgb;
    vec3 radiance = texture(uScene, uv).rgb + uComposite.params.y * bloom;
    vec3 colour = pow(vec3(1.0) - exp(-max(radiance, vec3(0.0)) * uComposite.params.x), vec3(1.0 / 2.2));
    fragColour = vec4(colour * uComposite.blend.y, uComposite.blend.x);
}
