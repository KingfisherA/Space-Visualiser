#ifndef SV_CORONA_PUSH_CONSTANTS_GLSL
#define SV_CORONA_PUSH_CONSTANTS_GLSL

// Mirrors CoronaPushConstants in src/SV/Universe/Star.cpp
layout(push_constant) uniform CoronaPushConstants
{
    mat4 transform; // The star's model to world transform: translation, rotation and scale to its radius
    // rgb = photosphere colour (linear, luminance 1), or the stylised colour (StylisedColour in
    // StylisedStar.glsl), w = hydrogen emission intensity
    vec4 colour;
    vec4 activity;  // x = flare activity, y = 1 for the stylised star, z = brightness (fading in)
} uCorona;

// Half size of the billboard, in stellar radii
const float CORONA_EXTENT = 4.0;

#endif // SV_CORONA_PUSH_CONSTANTS_GLSL
