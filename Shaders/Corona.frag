#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "CoronaPushConstants.glsl"
#include "SolarActivity.glsl"
#include "StylisedStar.glsl"

layout(location = 0) in vec3 vWorldPosition;

// Added to the scene (alpha 0 leaves the target's alpha alone)
layout(location = 0) out vec4 fragColour;

// The light around a star's limb, as seen in a total solar eclipse:
//  - The K-corona: photospheric light Thomson scattered by free electrons. It is white, falls
//    steeply away from the limb, and is shaped by the magnetic field into broad, petal-like helmet
//    streamers that curve as they rise and narrow to rounded tips, fine rays at every position
//    angle, and plumes over the poles.
//  - Coronal loops: hot plasma held in arches of magnetic field over active regions, and over
//    flares, where they brighten suddenly and fade.
//  - The chromosphere and prominences: a thin layer, and hedgerows held up by magnetic fields, of
//    cooler gas whose hydrogen glows in the Balmer lines. Hydrogen excited to n = 3 falls to the
//    first excited state (n = 2), emitting H-alpha at 656.3 nm; the weaker H-beta (n = 4 to 2,
//    486.1 nm) and helium D3 (587.6 nm) turn that red to the pink seen at the limb in eclipses.
//    The prominences are the filaments that Star.frag draws dark against the disc (SolarActivity.glsl).
//
// Or, for the stylised star, the corona as a total eclipse shows it, in the star's colour, with the
// red hydrogen emission at the limb (StylisedCorona in StylisedStar.glsl).
//
// Positions are measured on the sky: b is a ray's closest approach to the star's centre (its
// impact parameter) in stellar radii, so the limb is at b = 1 wherever the camera is. Structure
// is sampled in the star's model space, so it turns with the star.

// Radiance at the limb of the K-corona, of the loops and of the hydrogen emission (in the units
// ToneMap expects)
const float K_CORONA_RADIANCE = 0.08;
const float ACTIVE_LOOP_RADIANCE = 0.08;
const float FLARE_LOOP_RADIANCE = 0.4;
const float CHROMOSPHERE_RADIANCE = 0.35;
const float PROMINENCE_RADIANCE = 0.8;

// Linear sRGB of H-alpha, H-beta and He D3 (the CIE 1931 colour matching functions at each
// wavelength), mixed 1 : 0.25 : 0.03 by energy as in prominences and normalised.
const vec3 HYDROGEN_EMISSION = vec3(1.0, 0.096, 0.277);

// Scale height of the chromosphere, and typical height of prominences, in stellar radii (the
// chromosphere is thickened from ~0.004 so that it reads on screen)
const float CHROMOSPHERE_HEIGHT = 0.012;
const float PROMINENCE_HEIGHT = 0.05;
// Heights of the loops over active regions, and of flare loops at rest and at a flare's peak
const float ACTIVE_LOOP_HEIGHT = 0.07;
const float FLARE_LOOP_HEIGHT = 0.05;
const float FLARE_LOOP_RISE = 0.06;
// Loops around the limb (even, so the arcade meets itself across the position angle's seam)
const float LOOP_COUNT = 120.0;

// Streamers: how far they bend (radians per stellar radius of height), how they extend outwards
// (a power of b), and their brightness over the poles relative to the equator (1 near solar
// maximum, as in the 1999 eclipse; lower near minimum)
const float STREAMER_BEND = 0.25;
const float STREAMER_REACH = 1.5;
const float POLAR_STREAMERS = 0.8;
// Depth of the fine rays' modulation
const float RAY_STRENGTH = 0.25;

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
    return pow(profile / 4.0432, 0.7);
}

// 1 while noise of this angular frequency (cycles per radian) is resolved, falling to 0 as its
// features shrink under a pixel of `pixelAngle` radians
float Resolved(float frequency, float pixelAngle)
{
    return 1.0 - smoothstep(0.25, 0.6, frequency * pixelAngle);
}

// An arcade of thin loops standing on the limb, of varied size and lean, about `loopHeight` high.
// `positionAngle` is in radians around the limb, and `height` above it in stellar radii.
float Arcade(float positionAngle, float height, float loopHeight, float seed)
{
    float x = positionAngle * (LOOP_COUNT / 6.28318531) + seed * LOOP_COUNT;
    vec4 hash = Hash44(vec4(floor(x), seed * 17.0, 3.0, 7.0));
    float up = height / (loopHeight * mix(0.4, 1.3, hash.y));
    float across = (fract(x) - 0.5) * mix(1.6, 2.6, hash.x) + (hash.w - 0.5) * up;
    float radius = sqrt(across * across + up * up);
    return exp(-pow((radius - 0.8) / 0.1, 2.0)) * smoothstep(0.4, 0.8, hash.z);
}

void main()
{
    vec3 centre = uCorona.transform[3].xyz;
    float starRadius = length(uCorona.transform[0].xyz);
    mat3 rotation = mat3(uCorona.transform) / starRadius;
    mat3 toModel = transpose(rotation);
    vec3 axis = rotation[1];
    float hydrogen = uCorona.colour.w;
    float flareActivity = uCorona.activity.x;
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

    // Direction on the sky from the centre (noise sampled on it alone draws radial structures)
    vec3 direction = offset / b;
    float pixelAngle = pixelHeight / b;

    // Fade out well before the billboard's edge
    float fade = 1.0 - smoothstep(0.5 * CORONA_EXTENT, 0.95 * CORONA_EXTENT, b);

    if (uCorona.activity.y > 0.5)
    {
        vec3 stylised = StylisedCorona(direction, ray, toModel, b, uCorona.colour.rgb, hydrogen, time, pixelHeight);
        fragColour = vec4(stylised * fade * uCorona.activity.z, 0.0);
        return;
    }

    // Streamers bend as they rise, following the field: turn the direction on the sky by an angle
    // that grows with height
    float drift = time * STREAMER_RATE;
    float bendNoise = noise(vec4(toModel * direction * 1.3 + 31.0, drift * 0.5), 2, 1.0, 0.5);
    float bendAngle = STREAMER_BEND * bendNoise * (b - 1.0);
    vec3 bent = direction * cos(bendAngle) + cross(ray, direction) * sin(bendAngle);
    vec3 bentModel = toModel * bent;
    float sinLatitude = bentModel.y;

    // Helmet streamers: broad petals over the limb that narrow to rounded tips
    float streamerNoise = 0.5 + 0.5 * noise(vec4(bentModel * 1.8, drift - 0.1 * b), 2, 1.0, 0.4);
    streamerNoise *= mix(POLAR_STREAMERS, 1.0, pow(1.0 - abs(sinLatitude), 1.5));
    float taper = b - 1.0;
    float petals = smoothstep(0.2 + 0.3 * taper, 0.55 + 0.25 * taper, streamerNoise);

    // Fine rays at every position angle
    float rays = snoise(vec4(bentModel * 18.0, drift * 0.3 + 5.0)) * 0.6 * Resolved(18.0, pixelAngle) +
                 snoise(vec4(bentModel * 55.0, drift * 0.3 + 9.0)) * 0.4 * Resolved(55.0, pixelAngle);
    float striation = max(1.0 + RAY_STRENGTH * smoothstep(1.0, 1.15, b) * rays, 0.0);

    // Plumes over the poles
    float plumes = (0.5 + 0.5 * snoise(vec4(bentModel * 14.0, drift * 0.5))) * pow(abs(sinLatitude), 4.0);

    float structure = (0.6 + 1.2 * petals * pow(b, STREAMER_REACH) + 0.4 * plumes) * striation;
    float kCorona = CoronaProfile(b) * mix(1.0, structure, smoothstep(1.0, 1.25, b)) * K_CORONA_RADIANCE;

    // Close to the limb: loops, the chromosphere and prominences
    float loopLight = 0.0;
    float emission = 0.0;
    if (b < 1.0 + 6.0 * PROMINENCE_HEIGHT)
    {
        float height = b - 1.0;
        // The point on the limb below, in model space, where the disc's activity is
        vec3 limb = toModel * direction;
        float activityNoise = ActivityNoise(limb, time);

        // Position angle on the sky, from the projected rotation axis (or any direction on the sky
        // when looking down the axis)
        vec3 north = axis - ray * dot(axis, ray);
        north = normalize(dot(north, north) > 1e-4 ? north : rotation[0] - ray * dot(rotation[0], ray));
        float positionAngle = atan(dot(direction, cross(ray, north)), dot(direction, north));
        // The loops are a few pixels wide when the star's radius is ~300 pixels; blur them to
        // their mean below that
        float loopsResolved = 1.0 - smoothstep(0.003, 0.008, pixelHeight);

        // Coronal loops over active regions
        float activeLoops = Arcade(positionAngle, height, ACTIVE_LOOP_HEIGHT, 0.0) +
                            Arcade(positionAngle, height, ACTIVE_LOOP_HEIGHT * 1.6, 0.37) * 0.6;
        activeLoops = mix(0.25, activeLoops, loopsResolved);
        loopLight = ActiveRegion(activityNoise) * activeLoops * ACTIVE_LOOP_RADIANCE * exp(-height / 0.1);

        // Flare loops, rising and glowing over a flare
        float flare = 0.0;
        float flareLoops = 0.0;
        if (flareActivity > 0.0)
        {
            flare = Flares(limb, time, activityNoise) * flareActivity;
            flareLoops = Arcade(positionAngle, height, FLARE_LOOP_HEIGHT + FLARE_LOOP_RISE * flare, 0.71);
            flareLoops = mix(0.3, flareLoops, loopsResolved);
            loopLight += flare * (flareLoops + 0.4) * FLARE_LOOP_RADIANCE * exp(-height / 0.08);
        }

        if (hydrogen > 0.0)
        {
            // The chromosphere, its edge broken into spicules (jets ~0.01 stellar radii high).
            // Widened to at least a pixel, keeping its total light, so it doesn't flicker when small.
            float spicules = 0.5 + 0.5 * snoise(vec4(limb * 160.0, time * SPICULE_RATE));
            float scaleHeight = CHROMOSPHERE_HEIGHT * (0.6 + 0.9 * spicules);
            float widened = max(scaleHeight, pixelHeight);
            float chromosphere = exp(-height / widened) * scaleHeight / widened * CHROMOSPHERE_RADIANCE;

            // Prominences: the filaments, seen over the limb as hedgerows of hydrogen threaded by
            // the field, with ragged tops
            float prominenceTime = time * PROMINENCE_RATE;
            float top = PROMINENCE_HEIGHT * (0.5 + snoise(vec4(limb * 9.0, prominenceTime * 0.1 + 70.0)) +
                                             0.5 * snoise(vec4(limb * 30.0, prominenceTime * 0.2 + 80.0)));
            float body = 1.0 - smoothstep(0.3, 1.0, height / max(top, 1e-4));
            float threads = 0.5 + 0.5 * noise(vec4(limb * 50.0, height * 6.0 - prominenceTime), 2, 1.0, 0.5);
            float knots = 0.5 + 0.5 * noise(vec4(limb * 12.0, height * 12.0 - prominenceTime), 2, 1.0, 0.5);
            float prominences = Filaments(limb, time, 0.08) * body * threads * knots * 2.0;

            // Loop prominences: hydrogen condensing and draining down the loops after a flare
            prominences += flare * flareLoops * 2.0 * exp(-height / PROMINENCE_HEIGHT);

            emission = (chromosphere + prominences * PROMINENCE_RADIANCE) * hydrogen;
        }
    }

    vec3 radiance = max(uCorona.colour.rgb * (kCorona + loopLight) + HYDROGEN_EMISSION * emission, 0.0) * fade;
    fragColour = vec4(ToneMap(radiance) * uCorona.activity.z, 0.0);
}
