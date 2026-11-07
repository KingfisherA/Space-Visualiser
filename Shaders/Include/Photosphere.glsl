#ifndef SV_PHOTOSPHERE_GLSL
#define SV_PHOTOSPHERE_GLSL

// A star's photosphere in visible light, shared by Star.frag (the Sun) and StarSystem.frag (the
// star test), so that every star is drawn by the same physics from its effective temperature:
//
//  - Limb darkening. Light leaving the surface at an angle whose cosine is mu comes from about
//    optical depth mu (the Eddington-Barbier approximation), where a grey atmosphere's temperature
//    is given by T^4 = 3/4 Teff^4 (tau + 2/3). Each point shines as a blackbody at that
//    temperature, so the edge is darker and redder than the centre. The effect is stronger in blue
//    light than in red, and stronger in cool stars (on the Wien side of their spectrum) than in hot
//    ones (on the Rayleigh-Jeans side). For the Sun this gives about 0.33 of the centre's green
//    light at the limb, as observed.
//  - Granulation. Below a star of about 7000 K or cooler, convection reaches the surface: hot gas
//    rises in the middle of each cell (granule) and cooler gas sinks in the narrow, darker lanes
//    between them. The Sun's granules are about 1500 km across (about 460 to a solar radius), and
//    each lasts about ten minutes. Their light varies by about 14% rms (3% in temperature). The
//    granules are larger in giants, and only a few cover a red supergiant. They fade out in stars
//    hotter than about 8000 K, whose outer layers carry heat by radiation instead.
//  - Starspots, where magnetic fields hold back convection. Each has a dark umbra about 0.4 of its
//    width, inside a penumbra of radial filaments. Spots on the Sun are about 1800 K cooler than
//    their surroundings; on cool stars the difference is smaller (Berdyugina 2005). The smallest
//    spots (pores) have no penumbra. Bright faculae surround them, and show mainly near the limb.
//
// Positions are unit vectors on the star (in a frame turning with it). A footprint is the size of a
// pixel on that unit sphere, so that detail smaller than a pixel fades to its mean and doesn't
// shimmer.

// Effective wavelengths of the sRGB primaries (nm). Each channel's light changes with temperature
// as a blackbody's does at that wavelength (Planck's law).
const vec3 CHANNEL_WAVELENGTHS = vec3(612.0, 549.0, 465.0);
const float SECOND_RADIATION_CONSTANT = 1.4388e7; // nm K

// Granulation: the temperature contrast (as a fraction of the star's temperature) per unit of
// Granulation's pattern, whose spread is about 0.5: about 3.2% rms, so about 14% in the Sun's green
// light, as simulations of its surface give (observations blur it to less)
const float GRANULATION_CONTRAST = 0.065;

// Spots: a grid of SPOT_CELLS cells per stellar radius. Each cell may have one spot site; the
// faculae around it reach FACULA_REACH times the spot's radius (all kept under half a cell; see
// Starspots).
const float SPOT_CELLS = 5.0;
const float MAX_PENUMBRA = 0.3;       // Radius, in cells
const float UMBRA_SHARE = 0.42;       // The umbra's radius over the penumbra's
const float PENUMBRA_COOLING = 0.3;   // Of the umbra's
const float PORE_SIZE = 0.25;         // Spots smaller than this (of the largest) are pores
const float FACULA_REACH = 2.6;
const float FACULA_WARMING = 0.025;   // At the limb (of the temperature)
const float PENUMBRAL_FILAMENTS = 41.0; // Around a spot

// Each channel's blackbody light at a temperature, relative to that at a reference temperature
vec3 PlanckRatio(float temperature, float reference)
{
    vec3 x = SECOND_RADIATION_CONSTANT / (CHANNEL_WAVELENGTHS * max(temperature, 300.0));
    vec3 x0 = SECOND_RADIATION_CONSTANT / (CHANNEL_WAVELENGTHS * max(reference, 300.0));
    // (exp(x0) - 1) / (exp(x) - 1), arranged so it can't overflow
    return (exp(x0 - x) - exp(-x)) / (1.0 - exp(-x));
}

// The temperature the light leaving at mu is emitted at (Eddington-Barbier, in a grey atmosphere).
// The disc's light averages out to a blackbody's at the effective temperature.
float EmergentTemperature(float temperature, float mu)
{
    return temperature * pow(0.75 * (clamp(mu, 0.0, 1.0) + 2.0 / 3.0), 0.25);
}

// The light (relative to a blackbody at the effective temperature `temperature`) leaving a point
// whose local temperature is `local`, at mu
vec3 PhotosphereLight(float temperature, float local, float mu)
{
    return PlanckRatio(EmergentTemperature(local, mu), temperature);
}

// Uniform random numbers in [0, 1) (Dave Hoskins' hash without sine)
vec3 PhotosphereHash(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

// How strongly a star of this temperature is granulated, relative to the Sun: weaker in red dwarfs,
// a little stronger in F stars, and fading out as convection stops reaching the surface
float GranulationStrength(float temperature)
{
    return mix(0.5, 1.0, smoothstep(3000.0, 5800.0, temperature)) * mix(1.0, 1.3, smoothstep(5800.0, 6800.0, temperature)) *
           (1.0 - smoothstep(7200.0, 8300.0, temperature));
}

// Granulation at p: the change in temperature, in units of GRANULATION_CONTRAST (about -0.6 in the
// lanes to 1.5 at the brightest granules' centres, averaging 0 with a spread of about 0.5). `cells`
// is the number of granules to a stellar radius, and `time` is in granule lifetimes.
float Granulation(vec3 p, float cells, float time, float footprint)
{
    float size = footprint * cells; // A pixel, in granules
    // (Faded out by the time a granule is about 3 pixels across, before its lanes alias into moire)
    float resolved = 1.0 - smoothstep(0.12, 0.35, size);
    if (resolved <= 0.0)
        return 0.0;

    // The nearest two granule centres (Worley noise). Each granule's centre wanders, and its
    // brightness waxes and wanes, over its life. Granules are of different sizes (distances are
    // weighted), which bends the lanes between them.
    vec3 q = p * cells;
    vec3 base = floor(q);
    float nearest = 1e9, second = 1e9;
    float brightness = 0.0;
    for (int i = 0; i < 27; i++)
    {
        vec3 cell = base + vec3(float(i % 3) - 1.0, float((i / 3) % 3) - 1.0, float(i / 9) - 1.0);
        vec3 h = PhotosphereHash(cell);
        float phase = 6.2831853 * (time * (0.8 + 0.4 * h.z) + h.x);
        vec3 wander = 0.18 * vec3(sin(phase + 6.2831853 * h.y), sin(phase * 0.7 + 6.2831853 * h.z), cos(phase * 0.9 + 6.2831853 * h.x));
        vec3 centre = cell + 0.5 + 0.6 * (h - 0.5) + wander;
        float d = length(q - centre) * (0.8 + 0.4 * PhotosphereHash(cell + 17.0).x);
        if (d < nearest)
        {
            second = nearest;
            nearest = d;
            brightness = 0.75 + 0.25 * sin(phase);
        }
        else if (d < second)
        {
            second = d;
        }
    }

    // Bright, domed granules, and dark lanes where they meet (widened as they shrink towards a pixel,
    // so they don't alias)
    float edge = second - nearest;
    float lane = smoothstep(0.0, 0.3 + 0.5 * size, edge);
    float dome = 1.0 - 0.6 * clamp(nearest / 0.8, 0.0, 1.0);
    float pattern = lane * dome * brightness;
    // (The pattern averages about 0.28, with a spread of 0.24)
    return (pattern - 0.28) * (0.5 / 0.24) * resolved;
}

// Implemented by the shader including this: how active a star's surface is around a spot site
// (a unit vector, 0 for none to 1 for the most), which sets whether there is a spot and how large
float SpotActivity(vec3 site);

// How much cooler a spot's umbra is than the surface around it (K), for a star at a temperature
float SpotCooling(float temperature)
{
    return clamp(mix(300.0, 1850.0, (temperature - 3300.0) / (5772.0 - 3300.0)), 200.0, 2000.0);
}

// Starspots at p: returns the temperature there as a fraction of the surface's (x), and how much
// facula there is (y, 0 to 1). `strength` scales the spots' contrast (0 for none).
vec2 Starspots(vec3 p, float temperature, float footprint, float strength)
{
    vec3 q = p * SPOT_CELLS;
    vec3 first = floor(q - 0.5);
    float factor = 1.0;
    float facula = 0.0;
    float umbralCooling = SpotCooling(temperature) / temperature * strength;
    float blur = footprint * SPOT_CELLS;

    // A site is kept only if it lies in its own cell, and a spot's faculae reach under half a cell,
    // so a spot within reach of q is in one of these 2 x 2 x 2 cells
    for (int i = 0; i < 8; i++)
    {
        vec3 cell = first + vec3(float(i & 1), float((i >> 1) & 1), float(i >> 2));
        vec3 h = PhotosphereHash(cell + 41.0);
        vec3 site = normalize(cell + h) * SPOT_CELLS;
        if (any(notEqual(floor(site), cell)))
            continue;
        vec3 n = site / SPOT_CELLS;
        float activity = SpotActivity(n);
        if (activity <= 0.02)
            continue;

        // Its size: from the activity around it, with a random spread
        float size = clamp(activity * (0.35 + 0.65 * h.x * h.x), 0.0, 1.0);
        float penumbra = size * MAX_PENUMBRA;
        float umbra = penumbra * UMBRA_SHARE;
        float reach = min(penumbra * FACULA_REACH, 0.49);
        vec3 offset = q - site;
        float d = length(offset);
        if (d > reach)
            continue;

        // Edges are sharp, but no sharper than a pixel
        float edge = max(blur, 0.03 * penumbra);
        float inUmbra = 1.0 - smoothstep(umbra - edge, umbra + edge, d);
        float hasPenumbra = smoothstep(PORE_SIZE * 0.6, PORE_SIZE, size);
        float inPenumbra = (1.0 - smoothstep(penumbra - edge, penumbra + edge, d)) * hasPenumbra;

        // The penumbra's filaments, running out from the umbra, fading out as they shrink under a pixel
        vec3 t1 = normalize(cross(n, vec3(0.31, 0.93, 0.17)));
        vec3 t2 = cross(n, t1);
        float angle = atan(dot(offset, t2), dot(offset, t1));
        float filaments = 0.5 + 0.5 * sin(angle * PENUMBRAL_FILAMENTS + 2.5 * sin(angle * 7.0 + 20.0 * h.y) + 9.0 * h.z);
        float spacing = 6.2831853 * max(d, 1e-4) / PENUMBRAL_FILAMENTS;
        filaments = mix(0.5, filaments, 1.0 - smoothstep(0.5, 1.5, blur / spacing));

        float penumbralCooling = umbralCooling * PENUMBRA_COOLING * (0.7 + 0.6 * filaments);
        float cooling = mix(penumbralCooling * inPenumbra, umbralCooling, inUmbra);
        factor = min(factor, 1.0 - cooling);
        facula = max(facula, (1.0 - smoothstep(penumbra, reach, d)) * (1.0 - inPenumbra) * activity * strength);
    }
    return vec2(factor, facula);
}

// The faculae's warming, as a fraction of the temperature: faint at the centre of the disc, and
// clearest towards the limb (where their hot walls are seen side on)
float FaculaWarming(float facula, float mu)
{
    return FACULA_WARMING * facula * (1.0 - smoothstep(0.1, 0.9, mu)) * smoothstep(0.0, 0.08, mu);
}

#endif // SV_PHOTOSPHERE_GLSL
