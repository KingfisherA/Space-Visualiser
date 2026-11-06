#ifndef SV_CORONA_PUSH_CONSTANTS_GLSL
#define SV_CORONA_PUSH_CONSTANTS_GLSL

// Mirrors CoronaPushConstants in src/SV/Universe/Star.cpp
layout(push_constant) uniform CoronaPushConstants
{
    mat4 transform; // The star's model to world transform: translation, rotation and scale to its radius
    vec4 colour;    // rgb = photosphere colour (linear, luminance 1), w = hydrogen emission intensity
    vec4 activity;  // x = flare activity
} uCorona;

// Half size of the billboard, in stellar radii
const float CORONA_EXTENT = 4.0;

#endif // SV_CORONA_PUSH_CONSTANTS_GLSL
