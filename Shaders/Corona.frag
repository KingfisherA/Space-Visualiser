#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "CoronaPushConstants.glsl"
#include "Maths/NoiseFunction.glsl"

layout(location = 0) in vec3 vWorldPosition;

// Added to the scene (alpha 0 leaves the target's alpha alone)
layout(location = 0) out vec4 fragColour;

// The light around a star's limb, as seen in a total solar eclipse:
//  - The K-corona: photospheric light Thomson scattered by free electrons. It is white, falls
//    steeply away from the limb, and is shaped by the magnetic field into helmet streamers over
//    the equatorial belts and fine plumes over the poles.
//  - The chromosphere and prominences: a thin layer, and clouds held up by magnetic loops, of
//    cooler gas whose hydrogen glows in the Balmer lines. Hydrogen excited to n = 3 falls to the
//    first excited state (n = 2), emitting H-alpha at 656.3 nm; the weaker H-beta (n = 4 to 2,
//    486.1 nm) and helium D3 (587.6 nm) turn that red to the pink seen at the limb in eclipses.
//
// Positions are measured on the sky: b is a ray's closest approach to the star's centre (its
// impact parameter) in solar radii, so the limb is at b = 1 wherever the camera is.

// Radiance at the limb of the K-corona and of the hydrogen emission (in the units ToneMap expects)
const float K_CORONA_RADIANCE = 0.02;
const float CHROMOSPHERE_RADIANCE = 0.35;
const float PROMINENCE_RADIANCE = 0.2;

// Linear sRGB of H-alpha, H-beta and He D3 (the CIE 1931 colour matching functions at each
// wavelength), mixed 1 : 0.25 : 0.03 by energy as in prominences and normalised.
const vec3 HYDROGEN_EMISSION = vec3(1.0, 0.096, 0.277);

// Scale height of the chromosphere, and of prominences, in solar radii (the chromosphere is
// thickened from ~0.004 so that it reads on screen)
const float CHROMOSPHERE_HEIGHT = 0.012;
const float PROMINENCE_HEIGHT = 0.08;

// Time scales, in units of uFrame.times.y
const float STREAMER_RATE = 1.0;
const float SPICULE_RATE = 20.0;
const float PROMINENCE_RATE = 3.0;

// The Baumbach (1937) brightness profile of the K-corona, relative to the limb. Eclipse
// photographs compress its huge range (as a radially graded filter does) so the streamers can be
// followed outwards; the exponent does the same here.
float CoronaProfile(float b)
{
    float profile = 1.425 * pow(b, -7.0) + 2.565 * pow(b, -17.0) + 0.0532 * pow(b, -2.5);
    return pow(profile / 4.0432, 0.55);
}

void main()
{
    float starRadius = uCorona.colour.w;
    vec3 centre = uCorona.transform[3].xyz;
    vec3 axis = uCorona.axis.xyz;
    float hydrogen = uCorona.axis.w;
    float time = uFrame.times.y;

    // The ray's closest approach to the centre
    vec3 camera = uFrame.cameraPosition.xyz;
    vec3 ray = normalize(vWorldPosition - camera);
    vec3 closest = camera + ray * dot(centre - camera, ray);
    vec3 offset = (closest - centre) / starRadius;
    float pixelHeight = fwidth(length(offset)); // Before any discard or branch
    // Inside the limb the star is drawn over the corona; carry on to the disc's polygonal edge
    float b = max(length(offset), 1.0);
    if (b > CORONA_EXTENT)
        discard; // Past the billboard's fade


    // Direction on the sky from the centre, fixed to the star (noise sampled on it alone draws
    // radial structures)
    vec3 direction = offset / b;
    float sinLatitude = dot(direction, axis);
    float equator = 1.0 - abs(sinLatitude);

    // K-corona. Helmet streamers over the equatorial belts taper outwards; plumes over the poles
    // are fine and faint.
    float drift = time * STREAMER_RATE - 0.15 * b;
    float streamers = 0.5 + 0.5 * noise(vec4(direction * 1.6, drift), 3, 1.0, 0.55);
    float helmets = smoothstep(0.3, 0.75, streamers) * mix(0.15, 1.0, pow(equator, 2.5 + b));
    float plumes = (0.5 + 0.5 * snoise(vec4(direction * 14.0, drift * 0.5))) * pow(abs(sinLatitude), 4.0);
    float structure = 0.35 + 1.4 * helmets + 0.5 * plumes;
    float kCorona = CoronaProfile(b) * mix(1.0, structure, smoothstep(1.0, 1.4, b)) * K_CORONA_RADIANCE;

    // Hydrogen emission, close to the limb
    float emission = 0.0;
    if (b < 1.0 + 6.0 * PROMINENCE_HEIGHT && hydrogen > 0.0)
    {
        float height = b - 1.0;

        // The chromosphere, its edge broken into spicules (jets ~0.01 solar radii high). Widened
        // to at least a pixel, keeping its total light, so it doesn't flicker when small.
        float spicules = 0.5 + 0.5 * snoise(vec4(direction * 160.0, time * SPICULE_RATE));
        float scaleHeight = CHROMOSPHERE_HEIGHT * (0.6 + 0.9 * spicules);
        float widened = max(scaleHeight, pixelHeight);
        float chromosphere = exp(-height / widened) * scaleHeight / widened * CHROMOSPHERE_RADIANCE;

        // Prominences: a few clouds of hydrogen along the limb, threaded by magnetic loops
        float footprints = snoise(vec4(direction * 3.0, time * PROMINENCE_RATE * 0.2 + 40.0));
        float clouds = noise(vec4(direction * 12.0, height * 18.0 - time * PROMINENCE_RATE), 2, 1.0, 0.5);
        float prominences = smoothstep(0.35, 0.7, footprints) * smoothstep(-0.1, 0.4, clouds) *
                            exp(-height / PROMINENCE_HEIGHT) * PROMINENCE_RADIANCE;

        emission = (chromosphere + prominences) * hydrogen;
    }

    // Fade out well before the billboard's edge
    float fade = 1.0 - smoothstep(0.75 * CORONA_EXTENT, 0.95 * CORONA_EXTENT, b);
    vec3 radiance = max(uCorona.colour.rgb * kCorona + HYDROGEN_EMISSION * emission, 0.0) * fade;
    fragColour = vec4(ToneMap(radiance), 0.0);
}
