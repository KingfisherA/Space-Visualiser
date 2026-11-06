#ifndef SV_STAR_PUSH_CONSTANTS_GLSL
#define SV_STAR_PUSH_CONSTANTS_GLSL

// Mirrors StarPushConstants in src/SV/Universe/Star.cpp
layout(push_constant) uniform StarPushConstants
{
    mat4 transform;  // Model to world: translation, rotation and scale to the star's radius
    vec4 properties; // x = detail octaves (0 for a plain disc), y = flare activity
    vec4 colour;     // rgb = photosphere colour (linear, luminance 1)
} uStar;

#endif // SV_STAR_PUSH_CONSTANTS_GLSL
