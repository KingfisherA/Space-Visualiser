#ifndef SV_RINGS_PUSH_CONSTANTS_GLSL
#define SV_RINGS_PUSH_CONSTANTS_GLSL

// Mirrors SV::RingPushConstants
layout(push_constant) uniform RingPushConstants
{
    mat4 transform;   // Translation column is the planet's position
    vec4 properties;  // x = ring width, y = planet radius, z = inverse shadow softness
} uRings;

#endif // SV_RINGS_PUSH_CONSTANTS_GLSL
