#ifndef SV_SOLAR_ACTIVITY_GLSL
#define SV_SOLAR_ACTIVITY_GLSL

#include "Maths/NoiseFunction.glsl"

// The magnetic activity of a star's surface, shared by Star.frag (the disc) and Corona.frag (over
// the limb) so the two agree: a filament on the disc is a prominence as it rotates over the limb,
// and a flare lights the disc and the loops above it together.
//
// Positions are unit vectors in the star's model space (y is the rotation axis), and time is
// uFrame.times.y.

const float ACTIVITY_RATE = 0.4;
const float FILAMENT_FREQUENCY = 3.2;

// Flare sites: a grid of FLARE_CELLS cells per stellar radius over model space. Each cycle a site
// may light a flare at a random point on the surface within its cell.
const float FLARE_CELLS = 3.0;
const float FLARE_RATE = 1.5;    // Cycles per unit of time (a site flares every minute or so)
const float FLARE_RISE = 0.015;  // Of a cycle (about a second)
const float FLARE_DECAY = 0.12;  // Of a cycle (several seconds)
const float FLARE_SIZE = 0.45;   // Radius, in cells (under half a cell; see Flares)
const float FLARE_CHANCE = 0.3;  // Of a site staying quiet for a cycle

// Uniform random numbers in [0, 1) (Dave Hoskins' hash without sine)
vec4 Hash44(vec4 p4)
{
    p4 = fract(p4 * vec4(0.1031, 0.1030, 0.0973, 0.1099));
    p4 += dot(p4, p4.wzxy + 33.33);
    return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}

// Large scale activity noise, strongest in the belts either side of the equator where active
// regions form
float ActivityNoise(vec3 p, float time)
{
    float belts = exp(-pow((abs(p.y) - 0.33) / 0.2, 2.0));
    return noise(vec4(p * 2.0, time * ACTIVITY_RATE + 10.0), 2, 1.0, 0.5) * mix(0.6, 1.25, belts);
}

// Active regions: 0 outside, 1 inside
float ActiveRegion(float activityNoise)
{
    return smoothstep(0.08, 0.45, activityNoise);
}

// Filaments: ribbons of cool, dense gas held above the surface by the magnetic field. Seen against
// the disc they are dark; over the limb they are prominences. `width` widens the ribbons and lets
// more of them through (0 for the disc's).
float Filaments(vec3 p, float time, float width)
{
    float ridge = 1.0 - abs(snoise(vec4(p * FILAMENT_FREQUENCY + 20.0, time * ACTIVITY_RATE)));
    float ribbon = smoothstep(0.9 - width, 0.98 - width * 0.5, ridge);
    float mask = snoise(vec4(p * 2.0 + 40.0, time * ACTIVITY_RATE));
    return ribbon * smoothstep(0.2 - 2.0 * width, 0.5 - 2.0 * width, mask);
}

// Flares: sudden brightenings over active regions, rising in about a second and fading over
// several. Returns their brightness at p (0 to 1).
float Flares(vec3 p, float time, float activityNoise)
{
    vec3 q = p * FLARE_CELLS;
    vec3 first = floor(q - 0.5);
    float flare = 0.0;

    // A site is kept only if it lies in its own cell, so one within FLARE_SIZE (under half a cell)
    // of q is in one of these 2 x 2 x 2 cells
    for (int i = 0; i < 8; i++)
    {
        vec3 cell = first + vec3(float(i & 1), float((i >> 1) & 1), float(i >> 2));
        float cycle = time * FLARE_RATE + Hash44(vec4(cell, 0.5)).w * 50.0;
        vec4 hash = Hash44(vec4(cell, floor(cycle)) + 0.5);
        vec3 site = normalize(cell + hash.xyz) * FLARE_CELLS;
        if (hash.w < FLARE_CHANCE || any(notEqual(floor(site), cell)))
            continue;

        float u = fract(cycle);
        float pulse = smoothstep(0.0, FLARE_RISE, u) * exp(-max(u - FLARE_RISE, 0.0) / FLARE_DECAY);
        float kernel = 1.0 - smoothstep(0.0, FLARE_SIZE, length(q - site));
        flare = max(flare, kernel * pulse * mix(0.4, 1.0, fract(hash.w * 7.0)));
    }

    // Only over active regions
    return flare * smoothstep(-0.05, 0.3, activityNoise);
}

#endif // SV_SOLAR_ACTIVITY_GLSL
