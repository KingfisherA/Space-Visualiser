#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Procedural.glsl"
#include "SkyMap.glsl"

layout(location = 0) in vec2 vNdc;

// The sky map texel (see SkyMap.glsl)
layout(location = 0) out vec4 fragColour;

// Bakes the compact objects' sky into one mip level of its octahedral map (once, when it is first
// needed: see SkyMap in src/SV/Universe/CompactObject.cpp). Each level is made from the sky itself,
// with the detail finer than its texels left out (each octave of noise fades to its mean, and each
// galaxy is blurred to at least a texel), so its mips are as smooth as box filtered ones.
//
// The sky, in galactic coordinates (x towards the galactic centre, z the galactic north pole):
//   - the Milky Way's band: the light of its stars, in clouds, brightest and thickest towards
//     its central bulge, and threaded with dark lanes and clouds of dust that redden what they dim
//   - glowing nebulae along the band: red hydrogen clouds with teal oxygen hearts, blue
//     reflection nebulae and the wispy shells of supernova remnants
//   - distant galaxies, mostly far from the band, and a couple of large ones
//   - a faint glow over all of it
// The stars themselves are drawn over it by the ray tracer, as sharp as its pixels; the map tells
// it where they crowd.

// Mirrors SkyBakePushConstants in src/SV/Universe/CompactObject.cpp
layout(push_constant) uniform SkyBakePushConstants
{
    vec4 level; // x = this level's size (texels), y = the angle across one of its texels (radians)
} uBake;

const float PI = 3.14159265;
const float TAU = 6.28318531;

// The band: its starlight (radiance at its brightest, where its star clouds are thickest), its
// thickness (radians, as an exponential's scale) away from and towards the centre, and the size of
// the central bulge (radians along and across the band)
const float BAND_BRIGHTNESS = 0.035;
const float BAND_THICKNESS_OUTER = 0.08;
const float BAND_THICKNESS_INNER = 0.16;
const vec2 BULGE_SIZE = vec2(0.18, 0.11);
// How much the band's clouds and lanes are drawn out along it
const float BAND_STRETCH = 1.8;
// How much brighter and darker its star clouds are (log of the ratio, per unit of noise)
const float CLOUD_CONTRAST = 9.0;
const float BULGE_BRIGHTNESS = 0.025;
// The dust's optical depth at its thickest, and how much more it dims blue than red
const float DUST_DEPTH = 3.0;
const vec3 DUST_REDDENING = vec3(0.8, 1.0, 1.25);

// Nebulae along the band
const int NEBULAE = 11;
const float NEBULA_BRIGHTNESS = 0.1;
const vec3 HYDROGEN_ALPHA = vec3(1.0, 0.2, 0.35);
const vec3 OXYGEN_III = vec3(0.2, 0.8, 0.85);
const vec3 REFLECTION = vec3(0.35, 0.5, 1.0);

// Distant galaxies: in cells of a grid around the sphere (cells per radian), this fraction of which
// have one, from tiny to a few degrees across
const float GALAXY_CELLS = 16.0;
const float GALAXY_CHANCE = 0.18;
const float GALAXY_BRIGHTNESS = 0.05;

// The faint glow behind everything
const vec3 BACKGROUND_COLOUR = vec3(0.0006, 0.0009, 0.002);

// Scales the gradient noise to about value noise's spread
const float NOISE_SCALE = 1.0;

// Turns each octave's lattice away from the last's, so the value noise's grid doesn't show
const mat3 OCTAVE_ROTATION = mat3(0.00, 0.80, 0.60,
                                  -0.80, 0.36, -0.48,
                                  -0.60, -0.48, 0.64);

// The colour (luminance 1) of a blackbody at a temperature (K), from Planck's law at three wavelengths
vec3 StarColour(float temperature)
{
    const vec3 wavelength = vec3(610e-9, 550e-9, 465e-9);
    vec3 b = pow(wavelength / 550e-9, vec3(-5.0)) / (exp(1.4388e-2 / (wavelength * temperature)) - 1.0);
    return b / dot(b, vec3(0.2126, 0.7152, 0.0722));
}

// Gradient noise at p, in [0, 1] (about: it is mostly within 0.15 of its middle). Smoother than
// Procedural.glsl's value noise, without its lattice showing in the sky's large clouds.
float GradientNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = p - i;
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float corners[8];
    for (int c = 0; c < 8; c++)
    {
        vec3 corner = vec3(c & 1, (c >> 1) & 1, c >> 2);
        vec3 cell = i + corner;
        vec3 gradient = vec3(Hash(cell), Hash(cell + 19.1), Hash(cell + 47.3)) * 2.0 - 1.0;
        corners[c] = dot(gradient, f - corner);
    }
    float n = mix(mix(mix(corners[0], corners[1], u.x), mix(corners[2], corners[3], u.x), u.y),
                  mix(mix(corners[4], corners[5], u.x), mix(corners[6], corners[7], u.x), u.y), u.z);
    return clamp(0.5 + NOISE_SCALE * n, 0.0, 1.0);
}

// How much of a detail of this frequency (cycles per texel) survives in the map
float Resolved(float cyclesPerTexel)
{
    return 1.0 - smoothstep(0.25, 0.6, cyclesPerTexel);
}

// Octaves of value noise at p (noise units), in [0, 1], each fading to its mean once it is finer
// than the footprint (noise units across a texel)
float Fractal(vec3 p, int octaves, float gain, float footprint)
{
    float sum = 0.0;
    float total = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++)
    {
        sum += amplitude * mix(0.5, GradientNoise(p * frequency), Resolved(frequency * footprint));
        total += amplitude;
        amplitude *= gain;
        frequency *= 2.03;
        p = OCTAVE_ROTATION * p + vec3(1.7, 9.2, 4.3);
    }
    return sum / total;
}

// As Fractal, but each octave folded into ridges (bright where the noise crosses its middle): filaments
float Ridged(vec3 p, int octaves, float gain, float footprint)
{
    float sum = 0.0;
    float total = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++)
    {
        float ridge = 1.0 - abs(2.0 * GradientNoise(p * frequency) - 1.0);
        sum += amplitude * mix(0.53, ridge * ridge, Resolved(frequency * footprint));
        total += amplitude;
        amplitude *= gain;
        frequency *= 2.03;
        p = OCTAVE_ROTATION * p + vec3(5.3, 2.1, 7.7);
    }
    return sum / total;
}

// A unit vector at galactic longitude l and latitude b
vec3 GalacticDirection(float l, float b)
{
    return vec3(cos(b) * cos(l), cos(b) * sin(l), sin(b));
}

// Two directions across `centre`, at `angle` round it
void TangentFrame(vec3 centre, float angle, out vec3 major, out vec3 minor)
{
    vec3 t1 = normalize(cross(centre, abs(centre.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0)));
    vec3 t2 = cross(centre, t1);
    major = cos(angle) * t1 + sin(angle) * t2;
    minor = cross(centre, major);
}

// A galaxy seen at g: an exponential disc (scale length `scale`, radians) seen tilted so it looks
// `ratio` as wide as it is long, with a yellow core and bluer spiral arms, blurred to at least a
// texel (keeping its light). `arms` adds spiral arms and (when tilted) a dust lane.
vec3 GalaxyLight(vec3 g, vec3 centre, vec3 major, vec3 minor, float scale, float ratio, float arms, float seed)
{
    vec3 offset = g - centre;
    float blur = 0.5 * uBake.level.y;
    float scaleX = sqrt(scale * scale + blur * blur);
    float scaleY = sqrt(scale * scale * ratio * ratio + blur * blur);
    vec2 q = vec2(dot(offset, major) / scaleX, dot(offset, minor) / scaleY);
    float r = length(q);
    if (r > 6.0)
        return vec3(0.0);
    // The light the blur spread out
    float kept = scale * scale * ratio / (scaleX * scaleY);

    float disc = exp(-r);
    float core = exp(-(r * r) / 0.06);
    // Spiral arms winding out (logarithmic), as much as they are resolved
    float spiral = 1.0;
    float detail = arms * Resolved(uBake.level.y / scale);
    if (detail > 0.0)
    {
        float theta = atan(q.y, q.x);
        float wind = 2.0 * theta - 3.2 * log(r + 0.05) + seed * TAU;
        spiral = mix(1.0, 0.45 + 1.1 * pow(0.5 + 0.5 * cos(wind), 2.0), detail * smoothstep(0.3, 1.2, r));
        // A tilted disc's dust lane, across its near side
        float lane = exp(-pow((q.y + 0.18) / 0.12, 2.0)) * smoothstep(0.75, 0.25, ratio);
        spiral *= 1.0 - 0.7 * detail * lane * smoothstep(0.2, 0.8, r) * smoothstep(4.0, 1.5, r);
    }
    return kept * (disc * spiral * StarColour(7000.0) + 2.5 * core * StarColour(4300.0));
}

// The small galaxies in the cells of a grid around the sphere: the eight cells around g find all
// those near it (each lies in the cell's corner nearest the sphere, so each is found once)
vec3 GalaxyField(vec3 g)
{
    vec3 light = vec3(0.0);
    vec3 p = g * GALAXY_CELLS;
    vec3 base = floor(p - 0.5);
    for (int c = 0; c < 8; c++)
    {
        vec3 cell = base + vec3(c & 1, (c >> 1) & 1, c >> 2);
        if (Hash(cell + 101.3) > GALAXY_CHANCE)
            continue;
        vec3 position = cell + vec3(Hash(cell + 11.3), Hash(cell + 23.9), Hash(cell + 37.1));
        if (abs(length(position) - GALAXY_CELLS) > 0.5)
            continue;
        vec3 centre = normalize(position);
        // Most tiny, a few large
        float size = Hash(cell + 51.7);
        float scale = mix(0.0005, 0.0055, pow(size, 5.0));
        vec3 major;
        vec3 minor;
        TangentFrame(centre, TAU * Hash(cell + 63.3), major, minor);
        float ratio = mix(0.15, 1.0, Hash(cell + 77.7));
        float brightness = mix(0.3, 1.0, Hash(cell + 89.1));
        light += brightness * GalaxyLight(g, centre, major, minor, scale, ratio, smoothstep(0.6, 0.95, size), Hash(cell + 97.9));
    }
    return light;
}

// One nebula, the `index`th, seen at g: its light
vec3 Nebula(vec3 g, int index, float footprint)
{
    float i = float(index);
    float h0 = Hash(vec3(i, 1.7, 3.1));
    float h1 = Hash(vec3(i, 5.3, 2.9));
    float h2 = Hash(vec3(i, 8.1, 6.7));
    float h3 = Hash(vec3(i, 4.4, 9.3));
    float h4 = Hash(vec3(i, 7.6, 1.2));
    // Along the band, more towards the centre, and close to its middle
    float l = PI * (2.0 * h0 - 1.0) * mix(0.35, 1.0, h4);
    float b = 0.14 * (h1 - 0.5);
    float radius = mix(0.035, 0.16, h2 * h2);
    vec3 centre = GalacticDirection(l, b);
    float d = length(g - centre) / radius;
    if (d > 2.6)
        return vec3(0.0);

    // Gas warped into billows and threaded with filaments
    float noiseScale = 1.6 / radius;
    float fp = footprint * noiseScale;
    vec3 q = g * noiseScale + 13.7 * i;
    vec3 warp = vec3(Fractal(q, 4, 0.5, fp), Fractal(q + 5.2, 4, 0.5, fp), Fractal(q + 9.7, 4, 0.5, fp)) - 0.5;
    float gas = Fractal(q + 2.2 * warp, 6, 0.55, fp);
    float filaments = Ridged(1.8 * q + 2.8 * warp, 5, 0.6, 1.8 * fp);
    filaments *= filaments;

    vec3 colour;
    float light;
    if (h3 < 0.6)
    {
        // A hydrogen cloud lit by the hot young stars in its heart, which ionise its oxygen
        float shape = exp(-1.3 * d * d);
        float cloud = shape * smoothstep(0.3, 0.75, gas + 0.3 * shape);
        light = cloud + 0.8 * filaments * shape;
        float heart = exp(-3.5 * d * d) * smoothstep(0.4, 0.7, gas);
        colour = mix(HYDROGEN_ALPHA, OXYGEN_III, 0.8 * heart * step(0.25, h4));
        // Dark globules of dust in front of it
        light *= 1.0 - 0.75 * smoothstep(0.62, 0.78, Fractal(1.3 * q + 31.0, 5, 0.55, 1.3 * fp)) * shape;
    }
    else if (h3 < 0.82)
    {
        // Dust lit by a star near it: soft, blue
        float shape = exp(-1.8 * d * d);
        light = 0.55 * shape * smoothstep(0.25, 0.8, gas) + 0.25 * exp(-12.0 * d * d);
        colour = REFLECTION;
    }
    else
    {
        // A supernova remnant: a thin, broken shell of filaments
        float shell = exp(-pow((d - 0.85 - 0.25 * (gas - 0.5)) / 0.12, 2.0));
        light = 1.6 * shell * (0.25 + filaments) * smoothstep(0.35, 0.6, gas);
        colour = mix(HYDROGEN_ALPHA, OXYGEN_III, smoothstep(0.4, 0.65, Fractal(0.7 * q + 17.0, 3, 0.5, 0.7 * fp)));
    }
    return mix(0.5, 1.0, h4) * light * colour;
}

void main()
{
    vec3 g = OctahedralDecode(gl_FragCoord.xy / uBake.level.x);
    float footprint = uBake.level.y;

    // Galactic longitude (0 at the centre) and latitude, the band meandering a little
    float l = atan(g.y, g.x);
    float b = asin(clamp(g.z, -1.0, 1.0));
    float bw = b + 0.07 * (Fractal(g * 1.5 + 3.1, 3, 0.5, 1.5 * footprint) - 0.5);
    float centreward = 0.5 + 0.5 * cos(l);

    // The band's structure: swirled, and drawn out along it (its noise squeezed across it, so its
    // finest detail across it is BAND_STRETCH times finer)
    vec3 swirl = vec3(Fractal(g * 4.0 + 1.3, 4, 0.5, 4.0 * footprint),
                      Fractal(g * 4.0 + 7.1, 4, 0.5, 4.0 * footprint),
                      Fractal(g * 4.0 + 4.7, 4, 0.5, 4.0 * footprint)) - 0.5;
    vec3 s = (g + 0.06 * swirl) * vec3(1.0, 1.0, BAND_STRETCH);
    float stretched = BAND_STRETCH * footprint;

    // The band's starlight: an exponential profile with a brighter core and a faint wide halo, in
    // clouds of stars with brighter knots, and grainy with the stars too faint to resolve
    float thickness = mix(BAND_THICKNESS_OUTER, BAND_THICKNESS_INNER, centreward * centreward);
    float profile = 0.6 * exp(-abs(bw) / thickness) + 0.3 * exp(-(bw * bw) / (0.12 * thickness * thickness)) + 0.1 * exp(-abs(bw) / 0.3);
    float along = 0.3 + 0.7 * pow(centreward, 2.5);
    // (each a lognormal factor: soft-edged, with its mean 1)
    float clouds = exp(CLOUD_CONTRAST * (Fractal(s * 6.0, 8, 0.6, 6.0 * stretched) - 0.5) - 0.5 * pow(0.1 * CLOUD_CONTRAST, 2.0));
    float knots = exp(1.5 * (Fractal(s * 30.0 + 3.3, 4, 0.6, 30.0 * stretched) - 0.5));
    float grain = exp(1.2 * (Fractal(g * 220.0, 2, 0.6, 220.0 * footprint) - 0.5));
    float band = along * profile * clouds * knots * grain;
    float bulge = exp(-(l * l) / (BULGE_SIZE.x * BULGE_SIZE.x) - (b * b) / (BULGE_SIZE.y * BULGE_SIZE.y)) * sqrt(clouds) * grain;
    vec3 starlight = BAND_BRIGHTNESS * band * StarColour(mix(7000.0, 4800.0, centreward))
                   + BULGE_BRIGHTNESS * bulge * StarColour(4300.0);

    // Dust: dark lanes down the band's middle, splitting it in places, threaded with sinews, and
    // clouds reaching away from it
    float lanes = exp(-abs(bw + 0.01) / (0.7 * thickness));
    float rift = smoothstep(0.45, 0.6, Fractal(s * 4.0 + 7.3, 7, 0.6, 4.0 * stretched));
    float sinews = smoothstep(0.35, 0.75, Ridged(s * 10.0 + 1.1, 6, 0.6, 10.0 * stretched));
    float reach = exp(-abs(bw) / 0.25) * smoothstep(0.55, 0.72, Fractal(s * 2.5 + 11.0, 6, 0.6, 2.5 * stretched));
    float depth = DUST_DEPTH * (lanes * (rift + 0.6 * sinews) + 0.6 * reach * (0.4 + sinews) + 0.25 * sinews * smoothstep(0.1, 0.5, profile));
    vec3 transmittance = exp(-depth * DUST_REDDENING);

    // Nebulae, partly behind the dust
    vec3 nebulae = vec3(0.0);
    for (int i = 0; i < NEBULAE; i++)
        nebulae += Nebula(g, i, footprint);
    nebulae *= NEBULA_BRIGHTNESS * mix(vec3(1.0), transmittance, 0.6);

    // Galaxies, seen through the dust and fewer through the band's own glow; and two large ones
    vec3 galaxies = GalaxyField(g);
    vec3 major;
    vec3 minor;
    vec3 centre = GalacticDirection(2.3, -0.45);
    TangentFrame(centre, 0.6, major, minor);
    galaxies += 1.4 * GalaxyLight(g, centre, major, minor, 0.012, 0.32, 1.0, 0.3);
    centre = GalacticDirection(-2.1, 0.62);
    TangentFrame(centre, 2.2, major, minor);
    galaxies += GalaxyLight(g, centre, major, minor, 0.007, 0.85, 1.0, 0.7);
    galaxies *= GALAXY_BRIGHTNESS * transmittance * (1.0 - 0.8 * smoothstep(0.0, 0.6, profile));

    vec3 background = BACKGROUND_COLOUR * (0.5 + Fractal(g * 2.5, 4, 0.5, 2.5 * footprint));
    vec3 radiance = background + starlight * transmittance + nebulae + galaxies;

    // Where the resolved stars crowd: in the band's star clouds and the bulge, thinned by the dust
    float crowding = clamp(along * profile * sqrt(clouds) + bulge, 0.0, 1.0);
    crowding *= pow(dot(transmittance, vec3(0.2126, 0.7152, 0.0722)), 0.7);

    fragColour = vec4(sqrt(clamp(radiance / SKY_MAP_SCALE, 0.0, 1.0)), crowding);
}
