#ifndef SV_ATMOSPHERE_PRECOMPUTE_GLSL
#define SV_ATMOSPHERE_PRECOMPUTE_GLSL

// Shared by the precomputation passes: the atmosphere is set 0, pass inputs are set 1.
#include "Atmosphere/Parameters.glsl"

// Mirrors SV::Atmosphere::PrecomputePushConstants
layout(push_constant) uniform PrecomputePushConstants
{
    mat4 luminanceFromRadiance; // Upper 3x3 used
    int layer;                  // Depth slice of the 3D texture being rendered
    int scatteringOrder;
} uPass;

#define luminance_from_radiance mat3(uPass.luminanceFromRadiance)

#endif // SV_ATMOSPHERE_PRECOMPUTE_GLSL
