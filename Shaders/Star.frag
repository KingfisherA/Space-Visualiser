#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "StarPushConstants.glsl"
#include "SolarActivity.glsl"

layout(location = 0) in vec3 vVertex;
layout(location = 1) in vec3 vCamVector;
layout(location = 2) in vec3 vNormal;

layout(location = 0) out vec4 fragColour;

// The star's surface as an artistic take on the Sun in extreme ultraviolet (as SDO images it near
// 30 nm), drawn in white rather than the false colour of those images: the tangled chromospheric
// network, active regions threaded by bright loops, dark coronal holes and filaments, flares, and
// a limb brightened (not darkened) because the glow is optically thin.

// Spatial frequencies on the unit sphere
const float NETWORK_FREQUENCY = 11.0;
const float LOOP_FREQUENCY = 9.0;
const float FLARE_RIBBON_FREQUENCY = 40.0;

// Time scales, in units of uFrame.times.y
const float NETWORK_RATE = 3.0;
const float FLARE_RIBBON_RATE = 10.0;

const float LIMB_BRIGHTENING = 0.8;
// Brightness at which the surface turns white, as 1 / exposure
const float SURFACE_EXPOSURE = 1.1;
// How much of the star's colour shows through the white (the Sun is white)
const float TINT = 0.25;

// Model space units per pixel, set in main()
float pixelSize;

// 1 while noise of this frequency is well resolved, falling to 0 as it shrinks under a pixel
float Resolved(float frequency)
{
    return 1.0 - smoothstep(0.3, 0.8, frequency * pixelSize);
}

// Ridged noise: 1 along thin, tangled lines
float Ridge(vec4 position)
{
    return 1.0 - abs(snoise(position));
}

void main()
{
    pixelSize = length(fwidth(vVertex)); // Before any branch

    vec3 photosphere = uStar.colour.rgb;
    vec3 tint = mix(vec3(1.0), photosphere / max(max(photosphere.r, photosphere.g), photosphere.b), TINT);
    int octaves = int(ceil(uStar.properties.x));
    float flareActivity = uStar.properties.y;
    if (octaves == 0)
    {
        fragColour = vec4(tint, 1.0);
        return;
    }

    vec3 p = normalize(vVertex);
    float time = uFrame.times.y;

    // Large scale: broad swells in brightness, active regions, and coronal holes nearer the poles
    float broad = noise(vec4(p * 3.0, time * ACTIVITY_RATE + 60.0), 2, 1.0, 0.5);
    float activityNoise = ActivityNoise(p, time);
    float activeRegion = ActiveRegion(activityNoise);
    float holeNoise = snoise(vec4(p * 1.4 + 7.0, time * ACTIVITY_RATE * 0.5)) + 0.25 * broad + 0.7 * (abs(p.y) - 0.55);
    float hole = smoothstep(0.35, 0.6, holeNoise) * (1.0 - activeRegion);

    // The chromospheric network: bright, tangled lanes around supergranules, at several scales.
    // Each octave fades to its mean as it shrinks under a pixel.
    vec4 position = vec4(p + 0.06 * broad, time * NETWORK_RATE);
    float network = 0.0;
    float frequency = NETWORK_FREQUENCY;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; i++)
    {
        float ridge = Ridge(position * frequency + float(i) * 3.7);
        network += mix(0.45, ridge * ridge, Resolved(frequency)) * amplitude;
        frequency *= 2.2;
        amplitude *= 0.6;
    }

    float brightness = 0.35 + 0.25 * broad + network;
    brightness *= mix(1.0, 0.35, hole);

    // Active regions: plage threaded by bright loops, with intense cores
    float loops = pow(Ridge(vec4(p * LOOP_FREQUENCY, time * ACTIVITY_RATE * 4.0) + 5.0), 6.0) * 0.6 +
                  pow(Ridge(vec4(p * LOOP_FREQUENCY * 2.3, time * ACTIVITY_RATE * 6.0) + 9.0), 8.0) * 0.4 *
                  Resolved(LOOP_FREQUENCY * 2.3);
    brightness += activeRegion * (0.7 + 2.5 * loops) + 2.0 * smoothstep(0.45, 0.85, activityNoise);

    // Filaments: long dark ribbons of cooler gas held above the surface
    brightness *= 1.0 - 0.55 * Filaments(p, time, 0.0) * Resolved(FILAMENT_FREQUENCY * 8.0);

    // Flares: a white glow over the active region, threaded by bright ribbons at the footpoints of
    // the flaring loops
    if (flareActivity > 0.0)
    {
        float flare = Flares(p, time, activityNoise) * flareActivity;
        float ribbons = 0.4 + 0.6 * pow(Ridge(vec4(p * FLARE_RIBBON_FREQUENCY, time * FLARE_RIBBON_RATE) + 3.0), 4.0);
        brightness += flare * (2.0 + 10.0 * flare * ribbons);
    }

    // The limb, seen through more of the optically thin glow, is brighter
    float mu = clamp(dot(normalize(vNormal), normalize(vCamVector)), 0.0, 1.0);
    brightness *= 1.0 + LIMB_BRIGHTENING * pow(1.0 - mu, 3.0);

    // Bright regions roll off smoothly to white
    fragColour = vec4(tint * (1.0 - exp(-max(brightness, 0.0) * SURFACE_EXPOSURE)), 1.0);
}
