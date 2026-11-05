#ifndef SV_CORONA_PUSH_CONSTANTS_GLSL
#define SV_CORONA_PUSH_CONSTANTS_GLSL

// Mirrors CoronaPushConstants in src/SV/Universe/Star.cpp
layout(push_constant) uniform CoronaPushConstants
{
    mat4 transform; // Translation and scale of the billboard: its half size is CORONA_EXTENT solar radii
    vec4 colour;    // rgb = photosphere colour (linear, luminance 1), w = the star's radius (world units)
    vec4 axis;      // xyz = the star's rotation axis (world space), w = hydrogen emission intensity
} uCorona;

// Half size of the billboard, in solar radii
const float CORONA_EXTENT = 4.0;

#endif // SV_CORONA_PUSH_CONSTANTS_GLSL
