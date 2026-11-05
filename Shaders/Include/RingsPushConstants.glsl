#ifndef SV_RINGS_PUSH_CONSTANTS_GLSL
#define SV_RINGS_PUSH_CONSTANTS_GLSL

// Mirrors SV::RingPushConstants (128 bytes)
layout(push_constant) uniform RingPushConstants
{
    mat4 transform; // Half ring mesh to world. The translation is the planet's centre.
    // Columns of the planet's rotation (planet space to world). w components:
    //   [0].w = world units per equatorial radius, [1].w = polar radius / equatorial radius
    vec4 axes[3];
    vec4 rings;     // x = inner radius, y = outer radius (equatorial radii)
} uRings;

mat3 GetRingsPlanetRotation()
{
    return mat3(uRings.axes[0].xyz, uRings.axes[1].xyz, uRings.axes[2].xyz);
}

#endif // SV_RINGS_PUSH_CONSTANTS_GLSL
