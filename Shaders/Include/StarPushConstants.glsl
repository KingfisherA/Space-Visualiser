#ifndef SV_STAR_PUSH_CONSTANTS_GLSL
#define SV_STAR_PUSH_CONSTANTS_GLSL

// Mirrors StarPushConstants in src/SV/Universe/Star.cpp
layout(push_constant) uniform StarPushConstants
{
    mat4 transform;  // Model to world: translation, rotation and scale to the star's radius
    vec4 properties; // x = detail octaves (0 for a plain disc), y = flare activity, z = brightness (fading in)
    vec4 colour;     // rgb = photosphere colour (linear, luminance 1)
    // x = effective temperature (K), y = granules per stellar radius, z = spot contrast (0 for none),
    // w = how it is seen (0 in visible light, 1 in extreme ultraviolet, 2 stylised: then colour.rgb is
    // the stylised colour, StylisedColour in StylisedStar.glsl)
    vec4 surface;
} uStar;

#endif // SV_STAR_PUSH_CONSTANTS_GLSL
