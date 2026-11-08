#ifndef SV_STYLISED_STAR_GLSL
#define SV_STYLISED_STAR_GLSL

#include "Maths/NoiseFunction.glsl"
#include "SolarActivity.glsl"

// The stylised ("cinematic") star, shared by every star drawn this way: the Sun (Star.frag and
// Corona.frag), the catalogue's stars close up (the same shaders), and the star test
// (StarSystem.frag).
//
//  - The surface: a churning disc in the star's colour, threaded by fine, curling bright filaments
//    and drifting clouds, darker in broad basins, with a thin bright rim.
//  - The corona, as a total eclipse shows it: a bright inner ring, broad white petals (helmet
//    streamers) that billow and narrow to rounded tips, combed into fine striations, and the thin
//    red chromosphere and pink prominences of hydrogen's Balmer emission at the limb.
//
// It isn't physical. Colours are display values (0 to 1). A star's colour comes from its
// temperature, relative to the Sun's (StylisedColour), so the Sun is white, hotter stars blue and
// cooler ones orange to red. Positions are unit vectors in the star's model space (y is its
// rotation axis), so everything turns with the star, and time is uFrame.times.y (about 20 s).

// Time scales, in units of uFrame.times.y
const float STYLISED_CHURN_RATE = 1.5;    // The surface's turbulence
const float STYLISED_CLOUD_RATE = 0.6;    // The bright clouds over the surface
const float STYLISED_STREAMER_RATE = 0.6; // The corona's petals
const float STYLISED_SPICULE_RATE = 20.0;
const float STYLISED_PROMINENCE_RATE = 3.0;

// The filaments: base spatial frequency (on the unit sphere), octaves, and the mean each fades to
// when it shrinks under a pixel
const float STYLISED_FILAMENT_FREQUENCY = 15.0;
const int STYLISED_FILAMENT_OCTAVES = 4;
const float STYLISED_FILAMENT_MEAN = 0.08;

// How far the petals bend as they rise (radians per stellar radius of height)
const float STYLISED_STREAMER_BEND = 0.35;
// The hydrogen emission's colour (pink-red, as eclipse photographs show H-alpha with some H-beta),
// and its brightest, nearly white pink
const vec3 STYLISED_HYDROGEN = vec3(1.0, 0.16, 0.32);
const vec3 STYLISED_HYDROGEN_CORE = vec3(1.0, 0.62, 0.82);
// The chromosphere's scale height and the prominences' typical height (stellar radii)
const float STYLISED_CHROMOSPHERE_HEIGHT = 0.005;
const float STYLISED_PROMINENCE_HEIGHT = 0.05;

// A star's stylised colour from its blackbody colour (linear, any brightness) and the Sun's: their
// ratio, so the Sun is white, softened a little, with its brightest channel 1. Mirrored by
// StylisedStarColour in src/SV/Universe/Star.cpp.
vec3 StylisedColour(vec3 blackbody, vec3 sunBlackbody)
{
    vec3 ratio = max(blackbody / max(sunBlackbody, vec3(1e-6)), vec3(1e-6));
    return pow(ratio / max(max(ratio.r, ratio.g), ratio.b), vec3(0.6));
}

// The palette: the darkest gas, the surface's body, and the bright filaments (nearly white)
vec3 StylisedDeep(vec3 colour)
{
    return colour * mix(colour, vec3(1.0), 0.4) * 0.68;
}
vec3 StylisedBody(vec3 colour)
{
    return pow(colour, vec3(1.4)) * 0.86;
}
vec3 StylisedHighlight(vec3 colour)
{
    return mix(colour, vec3(1.0), 0.6);
}

// Maps a brightness (0 dark, 0.5 the surface's body, 1 and over bright to white) to a colour
vec3 StylisedRamp(vec3 colour, float brightness)
{
    vec3 c = mix(StylisedDeep(colour), StylisedBody(colour), smoothstep(0.0, 0.55, brightness));
    c = mix(c, StylisedHighlight(colour), smoothstep(0.5, 1.1, brightness));
    return mix(c, vec3(1.0), smoothstep(1.1, 1.8, brightness));
}

// 1 while noise of this frequency is resolved, falling to 0 as its features shrink under a pixel
// (`footprint`, in the same units)
float StylisedResolved(float frequency, float footprint)
{
    return 1.0 - smoothstep(0.3, 0.8, frequency * footprint);
}

// Ridged noise: 1 along thin, tangled lines
float StylisedRidge(vec4 position)
{
    return 1.0 - abs(snoise(position));
}

// The surface at p (a unit vector in model space), seen at mu (the cosine of the angle from the
// line of sight to the normal), as a display colour. `footprint` is a pixel on the unit sphere.
vec3 StylisedSurface(vec3 p, float mu, vec3 colour, float time, float footprint)
{
    float churn = time * STYLISED_CHURN_RATE;

    // The surface flows in broad eddies: warp everything below by a slowly turning field
    vec3 warp = vec3(snoise(vec4(p * 2.0, churn * 0.4)), snoise(vec4(p * 2.0 + 17.0, churn * 0.4)),
                     snoise(vec4(p * 2.0 + 31.0, churn * 0.4)));
    vec3 q = p + 0.07 * warp;
    // and in smaller eddies, which curl the filaments
    vec3 eddies = vec3(snoise(vec4(q * 7.0 + 3.0, churn * 0.8)), snoise(vec4(q * 7.0 + 23.0, churn * 0.8)),
                       snoise(vec4(q * 7.0 + 43.0, churn * 0.8)));

    // Broad mottling: darker basins between brighter swells
    float mottle = noise(vec4(q * 2.4 + 5.0, churn * 0.3), 3, 1.0, 0.55);

    // Filaments: bright, tangled threads at several scales, each octave curled by the one before.
    // Each fades to its mean as it shrinks under a pixel.
    float filaments = 0.0;
    float frequency = STYLISED_FILAMENT_FREQUENCY;
    float amplitude = 1.0;
    vec3 f = q + 0.04 * eddies;
    for (int i = 0; i < STYLISED_FILAMENT_OCTAVES; i++)
    {
        float ridge = StylisedRidge(vec4(f * frequency + float(i) * 5.3, churn * (1.0 + 0.6 * float(i))));
        float thread = pow(ridge, 12.0);
        filaments += mix(STYLISED_FILAMENT_MEAN, thread, StylisedResolved(frequency, footprint)) * amplitude;
        f += warp.zxy * (0.25 / frequency) * ridge;
        frequency *= 2.15;
        amplitude *= 0.5;
    }
    // Fewer in the dark basins
    filaments *= smoothstep(-0.7, 0.35, mottle);

    // Bright clouds drifting over the surface, gathering towards the limb
    float cloudNoise = noise(vec4(p * 1.7 + 40.0, time * STYLISED_CLOUD_RATE), 3, 1.0, 0.5);
    float clouds = smoothstep(0.05, 0.55, cloudNoise) * mix(0.5, 1.0, 1.0 - mu);

    float brightness = 0.42 + 0.5 * mottle + 1.0 * filaments + 0.8 * clouds;
    // Brighter towards the limb, and a thin bright rim (no thinner than a pixel or two)
    brightness += 0.3 * pow(1.0 - mu, 3.0);
    float rimWidth = max(0.03, 2.0 * footprint * max(mu, 0.05));
    brightness += 0.8 * exp(-mu / rimWidth);

    vec3 surface = StylisedRamp(colour, brightness);

    // The inner corona hazes the disc's edge
    float veil = 0.35 * pow(1.0 - mu, 5.0) + 0.35 * clouds * pow(1.0 - mu, 2.0);
    return surface + (StylisedHighlight(colour) - surface) * clamp(veil, 0.0, 1.0);
}

// The corona and the hydrogen emission around a star, as a display colour (to add over the sky).
// `direction` is the direction on the sky from the star's centre (a unit vector across `ray`, the
// line of sight), `toModel` turns world directions into the star's model space, and `b` is the
// ray's closest approach to the centre, in stellar radii (1 at the limb). `pixelHeight` is a pixel
// in stellar radii there, and `hydrogen` the brightness of the Balmer emission (0 for none).
vec3 StylisedCorona(vec3 direction, vec3 ray, mat3 toModel, float b, vec3 colour, float hydrogen, float time, float pixelHeight)
{
    float height = max(b - 1.0, 0.0);
    float pixelAngle = pixelHeight / b;
    float drift = time * STYLISED_STREAMER_RATE;
    vec3 limb = toModel * direction;

    // The petals bend as they rise, following the field: turn the direction on the sky by an angle
    // growing with height
    float bendNoise = noise(vec4(limb * 1.3 + 31.0, drift * 0.5), 2, 1.0, 0.5);
    float bendAngle = STYLISED_STREAMER_BEND * bendNoise * height;
    vec3 bent = toModel * (direction * cos(bendAngle) + cross(ray, direction) * sin(bendAngle));

    // Petals: broad lobes over the limb that narrow to rounded tips as they rise
    float petalNoise = noise(vec4(bent * 1.5 + 7.0, drift - 0.3 * height), 3, 1.0, 0.5);
    float petals = smoothstep(-0.3 + 0.35 * height, 0.4 + 0.2 * height, petalNoise);
    // Each reaches its own height (0.5 to 1.4 stellar radii), ending in a rounded, fading tip
    float reach = 0.5 + 0.9 * smoothstep(-0.5, 0.5, noise(vec4(limb * 2.0 + 50.0, drift * 0.5), 2, 1.0, 0.5));
    petals *= 1.0 - smoothstep(0.45, 1.0, height / reach);
    // Their edges billow: soft clouds that change with height, drifting outwards
    float billow = 0.55 + 0.65 * smoothstep(-0.5, 0.5, noise(vec4(bent * (3.0 + 2.0 * height) + 3.0, 3.0 * height - drift * 0.7), 3, 1.0, 0.5));
    // Fine striations, combed out along the field (fading out as they shrink under a pixel)
    float rays = 0.6 * snoise(vec4(bent * 22.0, drift * 0.3 + 5.0)) * StylisedResolved(22.0, pixelAngle) +
                 0.4 * snoise(vec4(bent * 60.0, drift * 0.3 + 9.0)) * StylisedResolved(60.0, pixelAngle);
    float striation = 1.0 + 0.12 * rays * smoothstep(0.0, 0.1, height);

    // A bright ring hugging the limb, and the petals reaching out a stellar radius or more
    float inner = 0.9 * exp(-height / 0.05) + 0.8 * exp(-height / 0.25);
    float outer = 1.7 * petals * billow * exp(-height / 0.6);
    float light = (inner + outer) * striation;
    // White nearest the limb, in the star's colour further out
    vec3 tint = mix(colour, vec3(1.0), 0.6 * exp(-height / 0.15));
    vec3 corona = vec3(1.0) - exp(-light * tint);

    // The chromosphere and prominences: thin pink-red hydrogen over the corona
    if (hydrogen <= 0.0 || height > 6.0 * STYLISED_PROMINENCE_HEIGHT)
        return corona;

    // The chromosphere, its edge broken into spicules. Widened to at least a pixel, keeping its
    // total light, so it doesn't flicker when small.
    float spicules = 0.5 + 0.5 * snoise(vec4(limb * 160.0, time * STYLISED_SPICULE_RATE));
    float scaleHeight = STYLISED_CHROMOSPHERE_HEIGHT * (0.6 + 0.9 * spicules);
    float widened = max(scaleHeight, pixelHeight);
    float chromosphere = exp(-height / widened) * scaleHeight / widened * smoothstep(0.2, 0.8, spicules);

    // Prominences: the filaments seen over the limb as hedgerows of hydrogen threaded by the field,
    // with ragged tops (SolarActivity.glsl, as the physical corona draws them)
    float prominenceTime = time * STYLISED_PROMINENCE_RATE;
    float top = STYLISED_PROMINENCE_HEIGHT * (0.5 + snoise(vec4(limb * 9.0, prominenceTime * 0.1 + 70.0)) +
                                              0.5 * snoise(vec4(limb * 30.0, prominenceTime * 0.2 + 80.0)));
    float body = 1.0 - smoothstep(0.3, 1.0, height / max(top, 1e-4));
    float threads = 0.5 + 0.5 * noise(vec4(limb * 50.0, height * 6.0 - prominenceTime), 2, 1.0, 0.5);
    float knots = 0.5 + 0.5 * noise(vec4(limb * 12.0, height * 12.0 - prominenceTime), 2, 1.0, 0.5);
    float prominences = Filaments(limb, time, 0.14) * body * threads * knots * 3.0;

    // Laid over the corona (it outshines it in red), whitening where it is brightest
    float emission = (0.6 * chromosphere + 1.6 * prominences) * hydrogen;
    vec3 pink = mix(STYLISED_HYDROGEN, STYLISED_HYDROGEN_CORE, smoothstep(1.0, 2.5, emission));
    return mix(corona, pink, 1.0 - exp(-2.0 * emission));
}

#endif // SV_STYLISED_STAR_GLSL
