#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Maths/NoiseFunction.glsl"
#include "Procedural.glsl"
#include "SkyMap.glsl"

layout(location = 0) in vec2 vNdc;

// Radiance (HDR)
layout(location = 0) out vec4 fragColour;

// Follows the pixel's ray of light back from the camera through the curved spacetime around a
// black hole or a neutron star (outside either, spacetime is the same: Schwarzschild's). Lengths
// are in Schwarzschild radii (a black hole's horizon is at r = 1) and times in the time light
// takes to cross one.
//
// Light's paths around the object are those of x'' = -3/2 h^2 x / r^5, where h = |x × x'| is
// constant along each path: the Newtonian form of the Schwarzschild orbit equation
// u'' + u = 3/2 u^2 (u = 1 / r). The parameter isn't the light's own time, so only the paths (not
// the speeds along them) mean anything. Each path is followed with RK4 steps sized to the distance
// from the object, until it falls through the horizon, lands on the neutron star or leaves for the sky.
//
// Along the way it collects the light of:
//   - the accretion disc, a thin disc in the XZ plane, wherever the ray crosses the plane. Rays
//     crossing it more than once show the disc's far side bent over and under the object, and
//     (around a black hole) the thin rings of light that circled it close to the photon sphere
//     (r = 1.5).
//   - a black hole's jets: glowing gas flowing out from the poles along the y axis
//   - a neutron star's beams, out from its magnetic poles, and the plasma glowing along its
//     magnetic field lines
// and, where it ends, the neutron star's boiling surface or the sky (lensed: bent around the
// object): the sky map baked once by Shaders/SkyBake.frag, with sharp stars drawn over it. A neutron star's glare is added last, from how close the ray came to it.

// Mirrors CompactObjectUniforms in src/SV/Universe/CompactObject.cpp (std140)
layout(std140, set = 1, binding = 0) uniform CompactObjectUniforms
{
    vec4 centre;   // xyz = the object's centre (render coordinates), w = time
    vec4 disc;     // x = inner radius, y = outer radius (0 for no disc), z = opacity, w = brightness
    vec4 discLook; // x = Doppler beaming, y = gravitational redshift, z = peak temperature (K), w = turbulence
    vec4 jets;     // x = brightness (0 for none), y = speed (c), z = tan(half opening angle), w = length
    vec4 sky;      // x = star brightness, y = sky brightness, z = pixel angle (radians), w = escape radius
    vec4 star;     // x = the neutron star's radius (0 for a black hole), y = brightness, z = temperature (K), w = cos(hot spot radius)
    vec4 magnetic; // xyz = the magnetic axis (unit), w = hot spot brightness
    vec4 beams;    // x = brightness (0 for none), y = half width (radians), z = length, w = magnetosphere brightness
    vec4 surface;  // xyz = a unit vector across the magnetic axis, turning with the star, w = surface detail
    vec4 glare;    // x = brightness, y = size
    vec4 skyMap;   // x = the angle across a texel of the sky map's first level (0 until it is baked), y = its last level
    vec4 blackbody[BLACKBODY_SAMPLES]; // rgb = colour (luminance 1)
} uObject;

// The sky, baked (see SkyMap.glsl)
layout(set = 1, binding = 1) uniform sampler2D uSkyMap;

const int MAX_STEPS = 500;
// Each step moves the ray this fraction of its distance from the object
const float STEP = 0.05;

// The disc's clumps are carried round at each radius's orbital speed, and are remade every
// FLOW_PERIOD (two sets, half a period apart, fading in and out) so they don't wind up forever.
// Mirrors TIME_WRAP in src/SV/Universe/CompactObject.cpp (a multiple of this).
const float FLOW_PERIOD = 30.0;
// The clumps' size: radians along their orbits, and fractions of their radius across them (in
// log r). Long, thin streaks.
const float STREAK_LENGTH = 0.8;
const float STREAK_WIDTH = 0.045;
// Peak of the thin disc temperature profile x^-3/4 (1 - x^-1/2)^1/4 (x = r / inner radius), at x = 49/36
const float PROFILE_PEAK = 0.4877;

// The jets' width where they leave the black hole, their colour (X-rays, in false colour) and
// the size of their knots
const float JET_BASE_WIDTH = 0.3;
const vec3 JET_COLOUR = vec3(0.35, 0.5, 1.0);
const float JET_KNOT_SIZE = 6.0;

// The neutron star's surface: boiling plasma, mottled and threaded with bright veins along the
// edges of cells of two sizes (cells per star radius). Its pattern changes at SURFACE_RATE (noise
// units per unit time) and is remade every SURFACE_PERIOD (two sets, half a period apart, fading in
// and out, as the disc's clumps are), so it loops when the time wraps.
const float SURFACE_PERIOD = 600.0;
const float SURFACE_RATE = 0.01;
const float SURFACE_CELLS = 5.0;
const float SURFACE_FINE_CELLS = 13.0;
// The veins' mean brightness, which each fades to as its cells shrink under a pixel
const float SURFACE_VEIN_MEAN = 0.2;
// Its hot spots' colour, and how much brighter they and the glare look when a beam sweeps past the camera
const vec3 HOT_SPOT_COLOUR = vec3(0.8, 0.9, 1.0);
const float PULSE_GAIN = 6.0;
const float GLARE_PULSE_GAIN = 1.5;

// The neutron star's glare, as its light scatters in a camera's lens: a tight, nearly white glow
// at the limb inside a wide blue halo, their widths in star radii (times the glare's size)
const vec3 GLARE_CORE_COLOUR = vec3(0.7, 0.85, 1.0);
const vec3 GLARE_HALO_COLOUR = vec3(0.3, 0.5, 1.0);
const float GLARE_CORE_WIDTH = 0.12;
const float GLARE_HALO_WIDTH = 0.6;
const float GLARE_HALO = 0.35;
// How much of the glare lies over the star's face (at its centre; all of it at the limb)
const float GLARE_OVER_STAR = 0.15;

// The beams (false colour: radio to X-rays): a blue glow around a white core (this fraction of
// the beam's width), with streaks flowing out along them: their length (star radii), speed and
// period (looping as the surface's)
const vec3 BEAM_COLOUR = vec3(0.45, 0.55, 1.0);
const vec3 BEAM_CORE_COLOUR = vec3(0.85, 0.9, 1.0);
const float BEAM_CORE_WIDTH = 0.3;
const float BEAM_STREAK_LENGTH = 3.0;
const float BEAM_FLOW_SPEED = 0.5;
const float BEAM_PERIOD = 200.0;

// The magnetic field lines: closed dipole loops r = L sin^2(angle from the magnetic axis), each at
// its own L (star radii) and angle around the axis. Some of the cells of a grid in (log L, angle
// around the axis) have one, jittered within the cell, so the lines are irregularly spaced. All of
// them are twisted round the axis a little from their middle to their ends (radians), and wobble.
// They fade out by FIELD_LINE_REACH star radii from the centre, so the largest loops show only as
// arcs leaving the poles.
const float FIELD_LINE_INNER = 1.25;   // The smallest loops' L
const float FIELD_LINE_OUTER = 14.0;   // The largest's
const float FIELD_LINE_SPACING = 1.28; // The ratio of L between neighbouring cells
const float FIELD_LINES_AROUND = 10.0;
const float FIELD_LINE_CHANCE = 0.55;  // Of a cell having a line
const float FIELD_LINE_JITTER = 0.3;   // Cells
const float FIELD_LINE_WOBBLE = 0.1;   // Cells
const float FIELD_LINE_TWIST = 0.6;
const float FIELD_LINE_REACH = 7.0;
// The lines' width (star radii, at the star; wider further out): a thin core inside a halo this
// many times wider, which adds this much (relative to the core) to a line's brightness
const float FIELD_LINE_WIDTH = 0.025;
const float FIELD_LINE_HALO_WIDTH = 4.0;
const float FIELD_LINE_HALO = 0.35;
// Their colours (false colour), and the pulses of plasma flowing along them to the poles: pulses
// per loop, and their period (a whole number of them in TIME_WRAP in src/SV/Universe/CompactObject.cpp)
const vec3 FIELD_LINE_COLOUR = vec3(0.55, 0.4, 1.0);
const vec3 FIELD_LINE_CORE_COLOUR = vec3(0.85, 0.8, 1.0);
const float FIELD_LINE_PULSES = 3.0;
const float FIELD_LINE_PULSE_PERIOD = 40.0;

// The stars drawn over the sky map: layers of them, each star in a cell of a grid around the sky's
// sphere (cells per radian), the cells this likely to have one where the sky is emptiest and this
// much more where its stars crowd most (in the band, as the map says), and their faintest and
// brightest (as a power of a random number, so most are faint)
const int STAR_LAYERS = 3;
const float STAR_CELLS[STAR_LAYERS] = float[](60.0, 170.0, 300.0);
const float STAR_CHANCE[STAR_LAYERS] = float[](0.1, 0.02, 0.0);
const float STAR_CROWDING[STAR_LAYERS] = float[](0.05, 0.3, 0.8);
const vec3 STAR_BRIGHTNESS[STAR_LAYERS] = vec3[](vec3(0.012, 2.5, 18.0), vec3(0.008, 0.3, 10.0), vec3(0.007, 0.04, 4.0));
// How much a lensed star can brighten, where the lensing stretches the sky across more pixels
const float MAX_MAGNIFICATION = 6.0;
const float TAU = 6.28318531;

vec3 Blackbody(float temperature)
{
    float x = BlackbodyIndex(temperature);
    int i = min(int(x), BLACKBODY_SAMPLES - 2);
    return mix(uObject.blackbody[i].rgb, uObject.blackbody[i + 1].rgb, x - float(i));
}

vec3 StarColour(float temperature)
{
    return Blackbody(temperature);
}

// The Doppler factor (observed / emitted frequency) of gas moving at `speed` (fraction of c) in
// direction `motion`, seen by light leaving it in direction `towardsCamera`
float DopplerFactor(float speed, vec3 motion, vec3 towardsCamera)
{
    float gamma = inversesqrt(1.0 - speed * speed);
    return 1.0 / (gamma * (1.0 - speed * dot(motion, towardsCamera)));
}

// How dense the disc's gas is at radius r and angle phi: streaks carried round with the gas
float DiscStructure(float r, float phi)
{
    // Kepler: the angular speed of a circular orbit (GM = 1/2 in these units)
    float angularSpeed = sqrt(0.5 / (r * r * r));
    float density = 0.0;
    for (int layer = 0; layer < 2; layer++)
    {
        float t = uObject.centre.w + float(layer) * 0.5 * FLOW_PERIOD;
        float cycle = floor(t / FLOW_PERIOD);
        float age = t - cycle * FLOW_PERIOD;
        float weight = 1.0 - abs(2.0 * age / FLOW_PERIOD - 1.0);
        float angle = phi - angularSpeed * age;
        // Around the orbit on a circle (so it joins up), across it in log r
        vec3 q = vec3(cos(angle) / STREAK_LENGTH, sin(angle) / STREAK_LENGTH, log(r) / STREAK_WIDTH) +
                 fract(cycle * vec3(0.1373, 0.7311, 0.5197)) * 97.0;
        density += weight * Fbm(q);
    }
    return density;
}

// Adds the disc's light where the ray (heading in `direction`) crosses it at `hit`, and dims what
// is behind it
void AddDisc(vec3 hit, vec3 direction, inout vec3 light, inout float transmittance)
{
    float inner = uObject.disc.x;
    float outer = uObject.disc.y;
    float r = length(hit.xz);
    if (r < inner || r > outer)
        return;

    // The streaks vary the gas's brightness more than its opacity, so the disc stays mostly solid
    float edges = smoothstep(inner, inner * 1.12, r) * (1.0 - smoothstep(mix(inner, outer, 0.4), outer, r));
    float structure = smoothstep(0.2, 0.8, DiscStructure(r, atan(hit.z, hit.x)));
    float turbulence = uObject.discLook.w;
    float alpha = clamp(uObject.disc.z * edges * mix(1.0, 0.4 + 0.6 * structure, turbulence), 0.0, 1.0);
    if (alpha <= 0.0)
        return;

    // The thin disc's temperature: falling outwards, and to nothing at the inner edge (where the gas
    // stops orbiting and falls in)
    float x = r / inner;
    float profile = pow(x, -0.75) * pow(max(1.0 - inversesqrt(x), 0.0), 0.25) / PROFILE_PEAK;
    float temperature = uObject.discLook.z * profile;

    // Seen from the camera: shifted by the gas's orbital motion (anticlockwise seen from +y, at its
    // speed measured by an observer at rest there) and by climbing out of the gravity
    vec3 orbit = vec3(-hit.z, 0.0, hit.x) / r;
    float speed = min(sqrt(0.5 / max(r - 1.0, 1e-3)), 0.99);
    float doppler = DopplerFactor(speed, orbit, -direction);
    float gravity = sqrt(1.0 - 1.0 / r);
    float shift = mix(1.0, doppler, uObject.discLook.x) * mix(1.0, gravity, uObject.discLook.y);

    // A blackbody's colour at the shifted temperature, and its brightness (bolometric, as T^4: the
    // shift's fourth power is the beaming)
    float observed = temperature * shift;
    float brightness = pow(observed / uObject.discLook.z, 4.0) * mix(1.0, 0.25 + 1.5 * structure, turbulence);
    light += transmittance * alpha * uObject.disc.w * brightness * Blackbody(observed);
    transmittance *= 1.0 - alpha;
}

// The jets' width at height h above the disc
float JetWidth(float h)
{
    return JET_BASE_WIDTH + h * uObject.jets.z;
}

// The light the jets give off per unit length at p, seen along `direction`
vec3 JetEmission(vec3 p, vec3 direction)
{
    float h = abs(p.y);
    float jetLength = uObject.jets.w;
    if (h < 1.0 || h > 2.0 * jetLength)
        return vec3(0.0);
    float width = JetWidth(h);
    float core = exp(-2.0 * dot(p.xz, p.xz) / (width * width));
    if (core < 1e-3)
        return vec3(0.0);

    float side = sign(p.y);
    float speed = uObject.jets.y;
    // Brightest at the base, fading along the jet
    float along = smoothstep(1.0, 3.0, h) * exp(-h / jetLength) * (1.0 + 4.0 * exp(-h / (0.1 * jetLength)));
    // Knots of brighter gas, flowing out
    float flow = (h - speed * uObject.centre.w) / JET_KNOT_SIZE;
    float knots = Noise(vec3(p.x / width, flow, p.z / width + side * 17.0));
    knots = 0.15 + 1.7 * knots * knots;
    // Beamed: brighter in the jet coming towards the camera (as D^3, for blobs of gas)
    float doppler = DopplerFactor(speed, vec3(0.0, side, 0.0), -direction);
    // Spread over the jet's width (a jet widening as it goes is fainter across)
    float spread = JET_BASE_WIDTH / width;
    vec3 colour = mix(JET_COLOUR, vec3(1.0), 0.5 * core * core);
    return colour * uObject.jets.x * core * along * knots * spread * doppler * doppler * doppler;
}

// A direction in the neutron star's own frame, turning with it, with y along the magnetic axis
vec3 StarFrame(vec3 v)
{
    vec3 axis = uObject.magnetic.xyz;
    vec3 across = uObject.surface.xyz;
    return vec3(dot(v, across), dot(v, axis), dot(v, cross(axis, across)));
}

// For things animated by noise that doesn't repeat: two copies (0 and 1), half a period apart,
// each remade every period and fading in and out, so they loop when the time wraps. Returns the
// copy's time into its period and its weight (the two's weights add to 1), and which period it's in.
vec2 LoopPhase(float period, int copy, out float cycle)
{
    float t = uObject.centre.w + float(copy) * 0.5 * period;
    cycle = floor(t / period);
    float age = t - cycle * period;
    return vec2(age, 1.0 - abs(2.0 * age / period - 1.0));
}

// An offset into noise for each period of a loop
vec3 LoopSeed(float cycle, int copy)
{
    return fract(cycle * vec3(0.1373, 0.7311, 0.5197) + float(copy) * vec3(0.37, 0.53, 0.29)) * 97.0;
}

// 1 while features of this frequency are resolved, falling to 0 as they shrink under a pixel
// (`footprint`, in the same units)
float Resolved(float frequency, float footprint)
{
    return 1.0 - smoothstep(0.1, 0.4, frequency * footprint);
}

// Cellular noise: the distances to the nearest and second nearest of a jittered grid of points
vec2 Cells(vec3 p)
{
    vec3 base = floor(p);
    vec3 f = p - base;
    vec2 d = vec2(8.0);
    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                vec3 o = vec3(x, y, z);
                vec3 cell = base + o;
                vec3 point = o + vec3(Hash(cell), Hash(cell + 17.3), Hash(cell + 41.9)) - f;
                float distance2 = dot(point, point);
                if (distance2 < d.x)
                {
                    d.y = d.x;
                    d.x = distance2;
                }
                else if (distance2 < d.y)
                {
                    d.y = distance2;
                }
            }
        }
    }
    return sqrt(d);
}

// 1, rising as one of the beams (along the magnetic axis) points along `direction`
float BeamPulse(vec3 direction, float gain)
{
    float angle = acos(min(abs(dot(uObject.magnetic.xyz, direction)), 1.0)) / uObject.beams.y;
    return 1.0 + gain * sign(uObject.beams.x) * exp(-angle * angle);
}

// The light the neutron star's beams give off per unit length at p: cones along the magnetic
// axis, both ways, fading out
vec3 BeamEmission(vec3 p)
{
    float radius = uObject.star.x;
    float r = length(p);
    if (r < radius)
        return vec3(0.0);
    vec3 b = StarFrame(p) / r;
    float across = acos(min(abs(b.y), 1.0)) / uObject.beams.y;
    if (across > 4.0)
        return vec3(0.0);
    float along = smoothstep(radius, 1.3 * radius, r) * exp(-(r - radius) / uObject.beams.z) * radius / r;

    // Streaks flowing out along them
    float streaks = 0.0;
    for (int copy = 0; copy < 2; copy++)
    {
        float cycle;
        vec2 phase = LoopPhase(BEAM_PERIOD, copy, cycle);
        vec3 q = vec3(1.5 * b.x / uObject.beams.y, (r - BEAM_FLOW_SPEED * phase.x) / (BEAM_STREAK_LENGTH * radius), 1.5 * b.z / uObject.beams.y);
        streaks += phase.y * Noise(q + sign(b.y) * 13.0 + LoopSeed(cycle, copy));
    }
    float glow = exp(-across * across) * (0.55 + 0.9 * streaks);
    float core = exp(-across * across / (BEAM_CORE_WIDTH * BEAM_CORE_WIDTH));
    return uObject.beams.x * along * (glow * BEAM_COLOUR + 2.0 * core * BEAM_CORE_COLOUR);
}

// The width of the neutron star's field lines' cores at distance r
float FieldLineWidth(float r)
{
    return FIELD_LINE_WIDTH * uObject.star.x * (0.5 + 0.5 * r / uObject.star.x);
}

// The light the plasma along the neutron star's field lines gives off per unit length at p, and
// how far p is from the nearest line (roughly: to size the ray's steps)
vec3 FieldLines(vec3 p, out float clearance)
{
    float radius = uObject.star.x;
    float r = length(p);
    vec3 b = StarFrame(p) / r;
    float c = b.y;                              // cos(angle from the magnetic axis)
    float s = sqrt(max(1.0 - c * c, 1e-6));     // and its sine
    float width = FieldLineWidth(r);

    // Where p is in the grid of cells: across the loops (log L) and around the axis (untwisted)
    float logSpacing = log(FIELD_LINE_SPACING);
    float u = log(r / (s * s * radius * FIELD_LINE_INNER)) / logSpacing;
    float v = (atan(b.z, b.x) - FIELD_LINE_TWIST * c) * FIELD_LINES_AROUND / TAU;
    float cellCount = log(FIELD_LINE_OUTER / FIELD_LINE_INNER) / logSpacing;
    // How far one cell is, here, across the loops (along the gradient of L) and around the axis
    float cellU = r * s * logSpacing * inversesqrt(1.0 + 3.0 * c * c);
    float cellV = r * s * TAU / FIELD_LINES_AROUND;
    // Lines in cells further than the 3 x 3 around p's are at least a cell away
    clearance = min(cellU, cellV);

    float pulseTime = uObject.centre.w * TAU / FIELD_LINE_PULSE_PERIOD;
    vec3 light = vec3(0.0);
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            vec2 cell = floor(vec2(u, v)) + vec2(i, j);
            if (cell.x < 0.0 || cell.x >= cellCount)
                continue;
            vec3 key = vec3(cell.x, mod(cell.y, FIELD_LINES_AROUND), 5.0);
            if (Hash(key) > FIELD_LINE_CHANCE)
                continue;
            vec4 h = vec4(Hash(key + 11.3), Hash(key + 23.9), Hash(key + 37.1), Hash(key + 51.7));
            float lineU = cell.x + 0.5 + FIELD_LINE_JITTER * (2.0 * h.x - 1.0);
            float lineV = cell.y + 0.5 + FIELD_LINE_JITTER * (2.0 * h.y - 1.0) + FIELD_LINE_WOBBLE * sin(4.0 * c + TAU * h.z);
            float du = (u - lineU) * cellU;
            float dv = (v - lineV) * cellV;
            float distance2 = du * du + dv * dv;
            clearance = min(clearance, sqrt(distance2));

            // A thin core in a soft halo, each adding up to about its brightness across the line
            float x2 = distance2 / (width * width);
            float core = exp(-x2);
            float halo = FIELD_LINE_HALO / FIELD_LINE_HALO_WIDTH * exp(-x2 / (FIELD_LINE_HALO_WIDTH * FIELD_LINE_HALO_WIDTH));
            // Some lines brighter than others, with pulses of plasma flowing along them to the poles
            float pulse = 0.65 + 0.35 * sin(FIELD_LINE_PULSES * 3.14159265 * abs(c) - pulseTime + TAU * h.w);
            float brightness = 0.3 + 0.7 * h.w * h.w;
            light += brightness * pulse * (core * FIELD_LINE_CORE_COLOUR + halo * FIELD_LINE_COLOUR);
        }
    }
    float fade = smoothstep(FIELD_LINE_REACH * radius, 0.4 * FIELD_LINE_REACH * radius, r) * radius / r;
    return light * uObject.beams.w * fade / (1.7725 * width);
}

// The neutron star surface's brightness at b (on the unit sphere, in the star's frame) at time t
// into a pattern seeded by `seed`: about 0.5 to 2, brightest along the veins. `footprint` is a
// pixel's size on the unit sphere.
float SurfacePattern(vec3 b, float t, vec3 seed, float footprint)
{
    float s = t * SURFACE_RATE;
    // The plasma flows in broad eddies: warp everything by a slowly changing field
    vec3 w = b * 2.0 + seed;
    vec3 warp = vec3(snoise(vec4(w, s)), snoise(vec4(w + 17.0, s)), snoise(vec4(w + 31.0, s)));
    vec3 q = b + 0.1 * warp + seed;

    // Broad mottling: dimmer basins between brighter swells
    float mottle = 0.5 + 0.5 * noise(vec4(q * 3.0, s * 2.0), 3, 1.0, 0.5);
    // Bright veins along the edges of the cells, and finer ones, curled further by the warp
    vec2 cells = Cells(q * SURFACE_CELLS);
    float veins = 1.0 - smoothstep(0.0, 0.15, cells.y - cells.x);
    vec2 fine = Cells(q * SURFACE_FINE_CELLS + 0.6 * warp);
    float fineVeins = 1.0 - smoothstep(0.0, 0.2, fine.y - fine.x);
    veins = mix(SURFACE_VEIN_MEAN, veins, Resolved(SURFACE_CELLS, footprint));
    fineVeins = mix(SURFACE_VEIN_MEAN, fineVeins, Resolved(SURFACE_FINE_CELLS, footprint));
    // Each cell a little brighter towards its middle
    float centre = mix(0.5, 1.0 - smoothstep(0.0, 0.6, cells.x), Resolved(SURFACE_CELLS, footprint));

    float detail = uObject.surface.w;
    return 0.3 + 0.45 * mottle + detail * (0.15 * centre + 0.9 * veins + 0.5 * fineVeins);
}

// The neutron star's surface where the ray (heading in `direction`) lands at normal n: blue-white
// plasma, white-hot along its veins and at its middle, and the hot spots at its magnetic poles
vec3 NeutronStarSurface(vec3 n, vec3 direction)
{
    float radius = uObject.star.x;
    // Climbing out of the star's gravity reddens and dims its light
    float shift = mix(1.0, sqrt(1.0 - 1.0 / radius), uObject.discLook.y);
    float dim = pow(shift, 4.0);
    float mu = max(dot(n, -direction), 0.0);

    // The plasma, in the star's frame so it turns with it
    vec3 b = StarFrame(n);
    float footprint = uObject.sky.z * length(uFrame.cameraPosition.xyz - uObject.centre.xyz) / radius;
    float level = 0.0;
    for (int copy = 0; copy < 2; copy++)
    {
        float cycle;
        vec2 phase = LoopPhase(SURFACE_PERIOD, copy, cycle);
        level += phase.y * SurfacePattern(b, phase.x, LoopSeed(cycle, copy), footprint);
    }

    // Darker towards the limb, where its light leaves through more of its atmosphere, and
    // white-hot in the middle of its face
    float limb = 0.25 + 0.75 * pow(mu, 0.7) + 0.6 * pow(mu, 4.0);
    float heat = level * limb;
    // Coloured from the star's temperature (saturated, so a hot star is a deep blue): deep where
    // it's dim, through the star's own colour, to white where it's hottest
    vec3 tint = Blackbody(uObject.star.z * shift);
    tint /= max(max(tint.r, tint.g), tint.b);
    vec3 colour = mix(0.5 * pow(tint, vec3(5.0)), pow(tint, vec3(3.0)), smoothstep(0.1, 0.7, heat));
    colour = mix(colour, mix(tint, vec3(1.0), 0.75), smoothstep(0.9, 2.0, heat));
    vec3 light = uObject.star.y * dim * heat * colour;

    // Hot spots at the magnetic poles, in a softer glow, flashing as a beam (along the axis) sweeps
    // past the light leaving here towards the camera
    float angle = acos(min(abs(b.y), 1.0));
    float spotRadius = acos(uObject.star.w);
    float spot = smoothstep(spotRadius, 0.4 * spotRadius, angle) + 0.3 * exp(-pow(angle / (2.0 * spotRadius), 2.0));
    light += spot * uObject.magnetic.w * BeamPulse(-direction, PULSE_GAIN) * dim * HOT_SPOT_COLOUR;
    return light;
}

// The neutron star's glare, around a ray that came as close as `closest` (star radii) to its centre
vec3 Glare(float closest)
{
    float x = max(closest - 1.0, 0.0) / max(uObject.glare.y, 1e-3);
    float core = exp(-x / GLARE_CORE_WIDTH);
    float halo = GLARE_HALO / (1.0 + (x * x) / (GLARE_HALO_WIDTH * GLARE_HALO_WIDTH));
    // Flaring as a beam sweeps past the camera
    float pulse = BeamPulse(normalize(uFrame.cameraPosition.xyz - uObject.centre.xyz), GLARE_PULSE_GAIN);
    return uObject.glare.x * pulse * (core * GLARE_CORE_COLOUR + halo * GLARE_HALO_COLOUR);
}

// The stars seen in a direction (galactic), where they crowd this much. Each is a Gaussian about a
// pixel across: `footprint` (radians) wide in the sky, the angle the pixel's rays spread over
// there. Where the lensing squeezes the sky into fewer pixels it is wider and fainter (keeping its
// light, which falls into fewer pixels, as the shrunken images of the sky do), and where it
// stretches the sky, narrower and brighter. A layer whose stars would be wider than its cells
// fades to its mean light, which is added as a glow.
vec3 Stars(vec3 direction, float crowding, float footprint)
{
    float pixel = max(uObject.sky.z, 1e-5);
    float width = max(footprint, pixel / sqrt(MAX_MAGNIFICATION));
    float magnification = (pixel * pixel) / (width * width);
    vec3 colour = vec3(0.0);
    for (int layer = 0; layer < STAR_LAYERS; layer++)
    {
        float cellsPerRadian = STAR_CELLS[layer];
        float chance = STAR_CHANCE[layer] + STAR_CROWDING[layer] * crowding * crowding;
        vec3 brightness = STAR_BRIGHTNESS[layer];
        float resolved = 1.0 - smoothstep(0.22, 0.4, width * cellsPerRadian);
        // Its mean light: stars per steradian times each one's (a mean brightness over pi pixel^2)
        float mean = brightness.x + brightness.y / (brightness.z + 1.0);
        colour += (1.0 - resolved) * chance * cellsPerRadian * cellsPerRadian * mean * 3.14159265 * pixel * pixel * StarColour(5500.0);
        if (resolved <= 0.0 || chance <= 0.0)
            continue;

        vec3 p = direction * cellsPerRadian;
        vec3 base = floor(p - 0.5);
        for (int c = 0; c < 8; c++)
        {
            vec3 cell = base + vec3(c & 1, (c >> 1) & 1, c >> 2) + float(layer) * 211.0;
            if (Hash(cell) > chance)
                continue;
            vec3 star = cell - float(layer) * 211.0 + vec3(Hash(cell + 11.3), Hash(cell + 23.9), Hash(cell + 37.1));
            if (abs(length(star) - cellsPerRadian) > 0.5)
                continue;
            float distance = length(direction - normalize(star));
            // Mostly cool yellow and orange stars, some hot blue-white ones
            float t = Hash(cell + 63.3);
            float temperature = t < 0.75 ? mix(3200.0, 6200.0, t / 0.75) : mix(6200.0, 16000.0, (t - 0.75) / 0.25);
            float light = brightness.x + brightness.y * pow(Hash(cell + 51.7), brightness.z);
            colour += resolved * magnification * light * StarColour(temperature) * exp(-(distance * distance) / (width * width));
        }
    }
    return colour;
}

// The sky seen in a direction, over a footprint (radians: see Stars)
vec3 Sky(vec3 direction, float footprint)
{
    if (uObject.skyMap.x <= 0.0)
        return vec3(0.0);
    vec3 g = ToGalactic(direction);
    float level = clamp(log2(max(footprint, 1e-6) / uObject.skyMap.x), 0.0, uObject.skyMap.y);
    vec4 texel = textureLod(uSkyMap, OctahedralEncode(g), level);
    return uObject.sky.y * SKY_MAP_SCALE * texel.rgb * texel.rgb + uObject.sky.x * Stars(g, texel.a, footprint);
}

vec3 Acceleration(vec3 x, float h2)
{
    float r = max(length(x), 0.5);
    float r2 = r * r;
    return -1.5 * h2 * x / (r2 * r2 * r);
}

void main()
{
    // The ray through this pixel, from the camera, around the object
    vec3 directionView = normalize(vec3(vNdc.x / uFrame.projection[0][0], vNdc.y / uFrame.projection[1][1], -1.0));
    vec3 position = uFrame.cameraPosition.xyz - uObject.centre.xyz;
    vec3 velocity = transpose(mat3(uFrame.view)) * directionView;
    vec3 angularMomentum = cross(position, velocity);
    float h2 = dot(angularMomentum, angularMomentum);

    bool jets = uObject.jets.x > 0.0;
    bool pulsar = uObject.star.x > 0.0;
    bool magnetosphere = uObject.beams.w > 0.0;
    // A black hole's horizon, or the neutron star's surface
    float surface = max(uObject.star.x, 1.0);
    float dither = Dither(gl_FragCoord.xy);
    float escapeRadius = max(uObject.sky.w, 1.5 * length(position));
    vec3 light = vec3(0.0);
    float transmittance = 1.0;
    bool escaped = false;
    // The nearest the ray comes to the centre, and (if it lands on the neutron star) the cosine of
    // its angle to the surface's normal there, for the glare
    float closest = escapeRadius;
    float landing = -1.0;
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float r = length(position);
        closest = min(closest, r);
        if (r < surface)
        {
            if (pulsar)
            {
                vec3 n = normalize(position);
                light += transmittance * NeutronStarSurface(n, normalize(velocity));
                landing = max(dot(n, -normalize(velocity)), 0.0);
            }
            break;
        }
        if (r > escapeRadius && dot(position, velocity) > 0.0)
        {
            escaped = true;
            break;
        }

        // Short enough to follow the bending, and not to step over the jets or field lines
        float stepLength = STEP * r;
        if (jets && abs(position.y) < 2.0 * uObject.jets.w)
        {
            float width = JetWidth(abs(position.y));
            stepLength = min(stepLength, max(0.4 * width, length(position.xz) - 2.5 * width));
        }
        // The field lines are sampled where each step starts, with steps short enough near them to
        // follow their profile (and not to step over them)
        vec3 lines = vec3(0.0);
        if (magnetosphere && r < FIELD_LINE_REACH * uObject.star.x)
        {
            float clearance;
            lines = FieldLines(position, clearance);
            stepLength = min(stepLength, max(0.5 * FieldLineWidth(r), 0.5 * clearance));
        }
        // Not into the star past its surface
        if (pulsar)
            stepLength = min(stepLength, max(r - surface, 0.02 * surface) + 0.02 * surface);
        float dt = stepLength / length(velocity);

        // RK4
        vec3 k1x = velocity;
        vec3 k1v = Acceleration(position, h2);
        vec3 k2x = velocity + 0.5 * dt * k1v;
        vec3 k2v = Acceleration(position + 0.5 * dt * k1x, h2);
        vec3 k3x = velocity + 0.5 * dt * k2v;
        vec3 k3v = Acceleration(position + 0.5 * dt * k2x, h2);
        vec3 k4x = velocity + dt * k3v;
        vec3 k4v = Acceleration(position + dt * k3x, h2);
        vec3 next = position + dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        vec3 nextVelocity = velocity + dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);

        vec3 midpoint = mix(position, next, dither);
        float segment = length(next - position);
        if (jets)
            light += transmittance * JetEmission(midpoint, normalize(velocity)) * segment;
        if (pulsar)
            light += transmittance * (lines + (uObject.beams.x > 0.0 ? BeamEmission(midpoint) : vec3(0.0))) * segment;
        if (position.y * next.y < 0.0)
        {
            float f = position.y / (position.y - next.y);
            AddDisc(mix(position, next, f), normalize(mix(velocity, nextVelocity, f)), light, transmittance);
        }

        position = next;
        velocity = nextVelocity;
        if (transmittance < 1e-3)
            break;
    }

    // Rays that neither fell in nor escaped circled close to the photon sphere: dark, like the
    // shadow they border. How far apart neighbouring pixels' rays end up (taken here, where every
    // pixel's ray has finished) is how much of the sky each sees, magnified or shrunk by the lensing.
    vec3 skyDirection = normalize(velocity);
    float footprint = max(length(dFdx(skyDirection)), length(dFdy(skyDirection)));
    if (escaped)
        light += transmittance * Sky(skyDirection, footprint);
    // The neutron star's glare, over everything (it's scattered in the lens): all of it around the
    // star, and over its face only a little, rising to all of it at the limb
    if (pulsar)
        light += Glare(closest / uObject.star.x) * (landing < 0.0 ? 1.0 : mix(GLARE_OVER_STAR, 1.0, pow(1.0 - landing, 3.0)));
    fragColour = vec4(light, 1.0);
}
