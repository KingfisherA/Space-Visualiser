#ifndef SV_FLARE_PUSH_CONSTANTS_GLSL
#define SV_FLARE_PUSH_CONSTANTS_GLSL

// Mirrors FlarePushConstants in src/SV/Universe/Star.cpp. Screen positions are in "view units":
// normalised device coordinates with x multiplied by the aspect ratio, so 1 is half the viewport's
// height in both directions and (0, 0) is its centre.
layout(push_constant) uniform FlarePushConstants
{
    vec4 star;   // xy = the star's centre (view units), z = its disc's radius (view units), w = aspect ratio
    vec4 params; // x = strength (0 to hide), y = size scale, z = a pixel (view units), w = lens ghost strength
    vec4 colour; // rgb = the photosphere's colour (linear, luminance 1)
} uFlare;

#endif // SV_FLARE_PUSH_CONSTANTS_GLSL
