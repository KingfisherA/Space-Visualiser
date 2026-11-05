#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "FlarePushConstants.glsl"

layout(location = 0) in vec2 vPosition;

// Added to the scene (alpha 0 leaves the target's alpha alone)
layout(location = 0) out vec4 fragColour;

// The glare of a star too small on screen to show its corona, as a camera sees the Sun: its
// light spread by the optics into a bright core and bloom, diffraction spikes from the aperture's
// six blades, fine starburst rays, a faint coloured halo, and ghosts (reflections between the
// lens's elements) along the line from the star through the centre of the view.
//
// Brightnesses are in "glare" units: ToneMap(glare * FLARE_RADIANCE) is about 0.8 at the default
// exposure (10) for a glare of 1. Distances are in view units (half the viewport's height), times
// the size scale for the glare, which shrinks as the star dims with distance.

const float FLARE_RADIANCE = 0.1;
const float PI = 3.14159265358979;

float Hash(float n)
{
    return fract(sin(n * 12.9898) * 43758.5453);
}

// Smooth noise around a circle (turn in [0, 1)), in `cells` cells
float AngularNoise(float turn, float cells, float seed)
{
    float x = turn * cells;
    float i = floor(x);
    float f = x - i;
    float a = Hash(mod(i, cells) + seed);
    float b = Hash(mod(i + 1.0, cells) + seed);
    return mix(a, b, f * f * (3.0 - 2.0 * f));
}

// A streak along the line through the star in direction `direction`, widened to at least about a
// pixel (keeping its total light) so it doesn't shimmer
float Spike(vec2 p, vec2 direction, float width, float length, float pixel)
{
    float along = abs(dot(p, direction));
    float across = abs(dot(p, vec2(-direction.y, direction.x)));
    float physical = width + along * 0.003;
    float drawn = max(physical, 0.6 * pixel);
    // A power law near the star, cut off further out (display gamma makes faint tails long)
    return physical / drawn * exp(-across / drawn) * pow(1.0 + along / length, -2.0) * exp(-along / (3.0 * length));
}

// A ghost: a soft disc with a brighter rim, centred at `centre`
float Ghost(vec2 position, vec2 centre, float radius)
{
    float d = length(position - centre) / radius;
    float rim = (d - 0.92) / 0.06;
    return smoothstep(1.0, 0.9, d) * (0.6 + 0.4 * d * d) + 0.5 * exp(-rim * rim);
}

void main()
{
    float strength = uFlare.params.x;
    float scale = uFlare.params.y;
    float pixel = uFlare.params.z;
    float ghostStrength = uFlare.params.w;

    vec2 p = vPosition - uFlare.star.xy;
    float r = length(p);
    float discRadius = max(uFlare.star.z, 1.5 * pixel);

    // The star itself, saturated, and the light scattered around it
    float core = 30.0 * exp(-(r * r) / (discRadius * discRadius));
    float bloom = pow(1.0 + r / (0.015 * scale), -2.0);
    float haze = 0.04 * exp(-r / (0.3 * scale));

    // Diffraction spikes: three lines (six points) from a six bladed aperture
    float spikes = 0.0;
    for (int i = 0; i < 3; i++)
    {
        float angle = 0.3 + float(i) * PI / 3.0;
        spikes += Spike(p, vec2(cos(angle), sin(angle)), 0.0015 * scale, 0.08 * scale, pixel);
    }
    spikes *= 0.8;

    // Starburst rays, faded out near the centre where they'd be finer than a pixel
    float turn = atan(p.y, p.x) / (2.0 * PI) + 0.5;
    float rayNoise = 0.6 * AngularNoise(turn, 61.0, 0.0) + 0.4 * AngularNoise(turn, 157.0, 17.0);
    float rays = 0.35 * pow(rayNoise, 3.0) * pow(1.0 + r / (0.05 * scale), -1.6) *
                 smoothstep(4.0 * pixel, 16.0 * pixel, r);

    vec3 glare = uFlare.colour.rgb * (core + bloom + haze + spikes + rays);

    // A faint halo, dispersed so red is outermost
    vec3 haloRadius = 0.42 * scale * vec3(1.03, 1.0, 0.97);
    vec3 haloOffset = (vec3(r) - haloRadius) / (0.012 * scale);
    glare += 0.012 * exp(-haloOffset * haloOffset);

    // Ghosts, mirrored through the centre of the view (where the lens's axis is)
    vec3 ghosts = vec3(0.0);
    if (ghostStrength > 0.0)
    {
        vec2 star = uFlare.star.xy;
        ghosts += vec3(1.0, 0.75, 0.45) * 0.025 * Ghost(vPosition, star * -0.25, 0.035);
        ghosts += vec3(0.45, 0.7, 1.0) * 0.010 * Ghost(vPosition, star * -0.55, 0.09);
        ghosts += vec3(0.6, 1.0, 0.6) * 0.030 * Ghost(vPosition, star * -0.9, 0.025);
        ghosts += vec3(0.7, 0.55, 1.0) * 0.006 * Ghost(vPosition, star * -1.3, 0.16);
        ghosts += vec3(1.0, 0.9, 0.7) * 0.025 * Ghost(vPosition, star * 0.45, 0.02);
    }

    vec3 radiance = (glare * strength + ghosts * ghostStrength) * FLARE_RADIANCE;
    fragColour = vec4(ToneMap(max(radiance, 0.0)), 0.0);
}
