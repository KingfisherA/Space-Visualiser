#ifndef SV_RINGS_PUSH_CONSTANTS_GLSL
#define SV_RINGS_PUSH_CONSTANTS_GLSL

// Mirrors SV::RingPushConstants (128 bytes)
layout(push_constant) uniform RingPushConstants
{
    mat4 transform; // Half ring mesh to world. The translation is the planet's centre.
    // Columns of the planet's rotation (planet space to world). w components:
    //   [0].w = world units per equatorial radius, [1].w = polar radius / equatorial radius
    vec4 axes[3];
    // x = inner radius, y = outer radius of the ring map (equal for none), z = outer radius of the
    // dust map (0 for none), all in equatorial radii. w = the dusty rings' brightness gain.
    vec4 rings;
} uRings;

mat3 GetRingsPlanetRotation()
{
    return mat3(uRings.axes[0].xyz, uRings.axes[1].xyz, uRings.axes[2].xyz);
}

#endif // SV_RINGS_PUSH_CONSTANTS_GLSL
