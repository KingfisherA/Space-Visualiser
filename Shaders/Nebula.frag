#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Procedural.glsl"

layout(location = 0) in vec2 vNdc;

// Radiance (HDR)
layout(location = 0) out vec4 fragColour;

// A nebula: the pixel's ray is marched through its gas (within a bounding sphere), adding the
// light the gas gives off and dimming what is behind by its dust (more in blue, so the dust
// reddens as well as dims). Then its stars, each dimmed by the dust between it and the camera, and
// the sky behind, dimmed by all of it. Lengths are in parsecs, about the nebula's centre.

const int MAX_STARS = 16;  // Mirrors MAX_NEBULA_STARS in src/SV/Universe/Nebula.hpp
const int MAX_CLOUDS = 12; // Mirrors MAX_NEBULA_CLOUDS in src/SV/Universe/Nebula.hpp
const int MAX_STEPS = 320; // Mirrors MAX_STEPS in src/SV/Universe/Nebula.cpp

// The kinds of nebula (mirrors SV::NebulaKind)
const int KIND_SUPERNOVA_REMNANT = 0;
const int KIND_HII_REGION = 1;
const int KIND_PILLARS = 2;
const int KIND_PLANETARY_NEBULA = 3;
const int KIND_SUPERNOVA_SHELL = 4;
const int KIND_EMISSION_NEBULA = 5;

// Mirrors NebulaUniforms in src/SV/Universe/Nebula.cpp (std140)
layout(std140, set = 1, binding = 0) uniform NebulaUniforms
{
    vec4 centre;  // xyz = the nebula's centre (render coordinates), w = time (s)
    vec4 shape;   // xyz = semi-axes, w = kind (KIND_...)
    vec4 form;    // x = tilt about z (radians), y = detail (noise tiles per pc), z = turbulence (pc), w = filament sharpness
    vec4 lines;   // x = hydrogen, y = oxygen, z = nitrogen and sulphur, w = continuum
    vec4 look;    // x = dust, y = filaments, z = star field brightness, w = pixel angle (radians)
    vec4 bounds;  // x = bounding radius, y = star count, z = steps, w = cloud count
    vec4 light;   // xyz = towards the stars lighting the pillars, w = the length of a step across the bounds (pc)
    vec4 stretch; // xyz = how much the structure is drawn out along each axis
    vec4 bubble;  // xyz = centre, w = radius (0 for none)
    vec4 bubbleShell; // x = thickness (pc), y = brightness, z = clearing, w = the share of [O III]
    // The emission lines' colours (luminance about 1): hydrogen, [O III], and [N II] and [S II]
    vec4 hydrogenColour;
    vec4 oxygenColour;
    vec4 sulphurColour;
    vec4 cloudBase[MAX_CLOUDS]; // xyz = base, w = radius there
    vec4 cloudHead[MAX_CLOUDS]; // xyz = head, w = radius there
    vec4 cloudInfo[MAX_CLOUDS]; // x = density
    vec4 stars[MAX_STARS];    // xyz = position, w = brightness
    vec4 starInfo[MAX_STARS]; // x = temperature (K), y = ionising radius (pc), z = ionising strength
    vec4 blackbody[BLACKBODY_SAMPLES]; // rgb = colour (luminance 1)
} uNebula;

// Four independent channels of tileable fBm, evenly spread over [0, 1]
layout(set = 1, binding = 1) uniform sampler3D uNoise;

// Stars are dimmed by the dust sampled at this many points between them and the camera
const int STAR_DUST_SAMPLES = 10;

#define HYDROGEN_COLOUR uNebula.hydrogenColour.rgb
#define OXYGEN_COLOUR uNebula.oxygenColour.rgb
#define SULPHUR_COLOUR uNebula.sulphurColour.rgb
// The synchrotron continuum's colour (blue-white)
const vec3 SYNCHROTRON_COLOUR = vec3(0.5, 0.8, 1.7);
// Dust absorbs blue light more than red, by about these ratios (for the dust depth in green)
const vec3 DUST_REDDENING = vec3(0.65, 1.0, 1.45);

// A supernova remnant: the shell its filaments lie in (fractions of its semi-axes), and the
// glow's wisps around the pulsar (their spacing, in fractions of the semi-axes, and their speed
// outwards, in the same per second)
const float SHELL_INNER = 0.35;
const float SHELL_OUTER = 1.0;
const float WISP_SPACING = 0.08;
const float WISP_SPEED = 0.01;

// An H II region: the molecular cloud's face behind the stars (its depth behind them, how much it
// bows towards the camera away from them and at most, pc), how thin the glowing layer on it is,
// and the dust of the cloud (per parsec at its face, thinning over WALL_THICKNESS behind it, so
// the cloud is a layer of finite depth: seen from the side or behind, the glow shows edge-on over
// it or dimly through it, rather than the cloud hiding all of it) and of the veil in front
const float WALL_DEPTH = 0.4;
const float WALL_CURVE = 0.06;
const float WALL_RISE = 0.7;
const float FRONT_THICKNESS = 0.35;
const float WALL_DUST = 9.0;
const float WALL_THICKNESS = 0.3;
const float VEIL_DEPTH = 0.9;
const float VEIL_THICKNESS = 0.35;
const float VEIL_DUST = 1.2;
// The Dark Bay: a dark lane of dust reaching in towards the Trapezium from the north-east (east
// is left, seen from +z)
const vec2 BAY_CENTRE = vec2(-0.75, 0.6);
const float BAY_RADIUS = 0.6;
const float BAY_DUST = 8.0;
// The gas away from the core is carved by large scale structure into wings and lanes (noise tiles
// per parsec, relative to the detail), except within this radius of the stars (pc)
const float CARVE_SCALE = 0.35;
const float CORE_RADIUS = 0.8;

// A cloud's surface is roughened by this fraction of its radius, its glowing rim facing the stars
// is this fraction of its radius thin (but no thinner than a few steps along the ray, so it is
// sampled smoothly), and a cloud of density 1 has this optical depth through its radius
const float CLOUD_ROUGHNESS = 0.3;
// The noise roughening a cloud's surface has tiles this many of its radii across (so a cloud of
// any size is as rough, and no finer than it can be drawn)
const float CLOUD_NOISE_SCALE = 3.0;
const float CLOUD_RIM = 0.06;
const float CLOUD_DEPTH = 8.0;

// An emission nebula: the light of its gas (across the nebula's largest semi-axis), of its clouds'
// rims and of the gas evaporating off them (likewise), and the optical depth of the dust mixed
// with its gas (likewise)
const float EMISSION_GLOW = 0.8;
const float RIM_GLOW = 0.5;
const float EVAPORATION_GLOW = 1.5;
// How far the gas evaporating off a cloud glows (a fraction of its radius)
const float CLOUD_EVAPORATION = 0.3;
const float GAS_DUST = 1.0;

// A planetary nebula: how thin its inner shell's wall is (a fraction of its radius), and its
// lobes' (of their radii), the size of the knots at their tips (of their radii), how bright its
// faint outer halo is, and the optical depth of its dust (across its largest semi-axis)
const float PLANETARY_WALL = 0.075;
const float LOBE_WALL = 0.2;
const float KNOT_SIZE = 0.55;
const float HALO_GLOW = 0.05;
const float PLANETARY_DUST = 0.4;
const float PLANETARY_GLOW = 0.45;

// A supernova shell: how far its front is pushed in by the denser clouds it has met (a fraction of
// its radius), and how much it is folded on smaller scales, and its glow (across its radius)
const float SHELL_RIPPLE = 0.16;
const float SHELL_FOLDS = 0.035;
const float SHELL_GLOW = 0.1;

// Pillars: how dense their dust is inside (per pc), how much their surfaces are roughened (pc),
// how thin the glowing skin on their lit faces is (pc), how far the gas evaporating off them glows
// (pc), and the glow of the gas around them (brighter on the cavity's far side, behind them)
const float PILLAR_DUST = 40.0;
const float PILLAR_ROUGHNESS = 0.09;
const float RIM_WIDTH = 0.035;
const float EVAPORATION_LENGTH = 0.12;
const float CAVITY_GLOW = 0.35;
const float BACK_GLOW = 1.6;

vec3 Blackbody(float temperature)
{
    float x = BlackbodyIndex(temperature);
    int i = min(int(x), BLACKBODY_SAMPLES - 2);
    return mix(uNebula.blackbody[i].rgb, uNebula.blackbody[i + 1].rgb, x - float(i));
}

vec3 StarColour(float temperature)
{
    return Blackbody(temperature);
}

vec4 NoiseAt(vec3 p)
{
    return textureLod(uNoise, p * uNebula.form.y, 0.0);
}

// 1 on the noise's middle value, falling to 0 at its extremes: thin sheets through the volume
float Ridge(float n)
{
    return 1.0 - abs(2.0 * n - 1.0);
}

// The distance from p to a cloud's surface (negative inside), the closest point on its axis, and
// how far along it that is (0 at its base, 1 at its head)
float CloudDistance(int i, vec3 p, out vec3 closest, out float along)
{
    vec3 base = uNebula.cloudBase[i].xyz;
    vec3 axis = uNebula.cloudHead[i].xyz - base;
    along = clamp(dot(p - base, axis) / max(dot(axis, axis), 1e-8), 0.0, 1.0);
    closest = base + along * axis;
    return length(p - closest) - mix(uNebula.cloudBase[i].w, uNebula.cloudHead[i].w, along);
}

// The light given off by ionised gas: hydrogen (with [N II] and [S II] where the light ionising
// it is softer) and [O III] (its share where the light is hardest)
vec3 IonisedGas(float oxygenShare, float sulphur)
{
    return uNebula.lines.x * HYDROGEN_COLOUR * (1.0 - 0.7 * oxygenShare) + uNebula.lines.y * OXYGEN_COLOUR * 1.6 * oxygenShare +
           uNebula.lines.z * SULPHUR_COLOUR * sulphur * (1.0 - oxygenShare);
}

// The nebula's frame: turned about z by its tilt
vec3 ToNebula(vec3 p)
{
    float c = cos(uNebula.form.x), s = sin(uNebula.form.x);
    return vec3(c * p.x + s * p.y, -s * p.x + c * p.y, p.z);
}

// The light given off (per parsec) and the dust (optical depth per parsec, in green) at p
void SupernovaRemnant(vec3 p, out vec3 emission, out float dust)
{
    vec3 u = p / uNebula.shape.xyz;
    float rho = length(u);
    emission = vec3(0.0);
    dust = 0.0;
    if (rho > 1.3)
        return;

    // Pushed about by the large scale noise, so the edges and filaments aren't regular
    vec4 warp = NoiseAt(p * 0.5 + 0.37);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;
    float rhoWarped = rho + (warp.a - 0.5) * 0.35;

    // The synchrotron glow: filling the remnant, brightest around the pulsar, in wisps moving out
    float wisps = 0.75 + 0.25 * sin((rho - WISP_SPEED * uNebula.centre.w) / WISP_SPACING * 6.2832 + 4.0 * warp.g);
    float glow = smoothstep(1.0, 0.15, rhoWarped) * (0.5 + warp.g) * wisps + 2.5 * exp(-dot(u, u) / 0.01);
    emission += uNebula.lines.w * SYNCHROTRON_COLOUR * 0.35 * glow;

    // The filaments: thin sheets of the noise in the shell, crossing into thin strands
    float shell = smoothstep(SHELL_INNER, SHELL_INNER + 0.3, rhoWarped) * (1.0 - smoothstep(SHELL_OUTER - 0.12, SHELL_OUTER + 0.05, rhoWarped));
    if (shell > 0.0)
    {
        vec4 n = NoiseAt(q);
        vec4 fine = NoiseAt(q * 2.3 + 11.0);
        float sheets = pow(Ridge(n.r), uNebula.form.w);
        float strands = sheets * (0.25 + 0.75 * pow(Ridge(fine.g), 3.0));
        float filament = shell * strands * uNebula.look.y;
        // Oxygen in some of them (green-blue), the rest hydrogen, nitrogen and sulphur
        float oxygen = smoothstep(0.5, 0.75, n.b);
        vec3 colour = uNebula.lines.x * HYDROGEN_COLOUR * (1.0 - 0.5 * oxygen) + uNebula.lines.z * SULPHUR_COLOUR * 0.5 * (1.0 - oxygen) +
                      uNebula.lines.y * OXYGEN_COLOUR * 1.5 * oxygen;
        emission += 6.0 * filament * colour;
        dust += 1.5 * filament;
    }
}

void HIIRegion(vec3 p, out vec3 emission, out float dust)
{
    vec3 u = p / uNebula.shape.xyz;
    float extent = 1.0 - smoothstep(0.7, 1.2, length(u));
    emission = vec3(0.0);
    dust = 0.0;

    vec4 warp = NoiseAt(p * 0.6 + 0.21);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;
    vec4 n = NoiseAt(q);

    // The cloud's face: bowed towards the camera away from the stars (no further than WALL_RISE,
    // so its rim doesn't stand up in front of the glow seen from the side), and rough. The cloud
    // reaches a little past the glowing gas across the sky, and thins away behind its face.
    float wallZ = -WALL_DEPTH + min(WALL_CURVE * dot(q.xy, q.xy), WALL_RISE) + (warp.a - 0.5) * 0.6;
    float inFront = p.z - wallZ;
    float cloudExtent = 1.0 - smoothstep(0.8, 1.25, length(u.xy));
    dust += WALL_DUST * smoothstep(0.0, -0.12, inFront) * exp(min(inFront, 0.0) / WALL_THICKNESS) * cloudExtent;

    // Ionised by the hot stars: densest on the cloud's face, in clumps
    float ionised = 0.0, oxygen = 0.0;
    int count = int(uNebula.bounds.y);
    for (int i = 0; i < MAX_STARS; i++)
    {
        if (i >= count)
            break;
        float radius = uNebula.starInfo[i].y;
        if (radius <= 0.0)
            continue;
        float d = distance(q, ToNebula(uNebula.stars[i].xyz));
        ionised += uNebula.starInfo[i].z * exp(-d / radius);
        oxygen += uNebula.starInfo[i].z * exp(-d / (0.4 * radius));
    }
    float front = exp(-max(inFront, 0.0) / FRONT_THICKNESS) * smoothstep(-0.15, 0.0, inFront);
    float clumps = 0.2 + 1.6 * n.r * n.r;
    vec4 large = NoiseAt(q * CARVE_SCALE + 2.7);
    float carve = mix(smoothstep(0.3, 0.7, large.b), 1.0, exp(-dot(q, q) / (CORE_RADIUS * CORE_RADIUS)));
    // Brightest close to the stars, where the light ionising the gas is strongest
    float gas = (0.05 + front) * (ionised + 3.0 * ionised * ionised) * clumps * extent * carve;
    float oxygenShare = clamp(oxygen / max(ionised, 1e-4), 0.0, 1.0);
    emission = 1.4 * gas * (uNebula.lines.x * HYDROGEN_COLOUR * (1.0 - 0.7 * oxygenShare) +
                            uNebula.lines.y * OXYGEN_COLOUR * 1.6 * oxygenShare +
                            uNebula.lines.z * SULPHUR_COLOUR * 0.4 * front * (1.0 - oxygenShare));
    // Starlight scattered off the dust near the stars (blue)
    emission += uNebula.lines.w * vec3(0.5, 0.75, 1.4) * front * ionised * extent * carve * (0.5 + n.g);

    // The veil of dust in front, in wisps, and thick in the Dark Bay
    float veil = exp(-pow((p.z - VEIL_DEPTH) / VEIL_THICKNESS, 2.0));
    vec4 v = NoiseAt(q * 0.8 + 5.3);
    float bay = exp(-dot(q.xy - BAY_CENTRE, q.xy - BAY_CENTRE) / (BAY_RADIUS * BAY_RADIUS));
    dust += veil * extent * (VEIL_DUST * smoothstep(0.45, 0.8, v.b) + BAY_DUST * bay * smoothstep(0.3, 0.6, v.a));
}

void Pillars(vec3 p, out vec3 emission, out float dust)
{
    vec3 u = p / uNebula.shape.xyz;
    float extent = 1.0 - smoothstep(0.75, 1.15, length(u));
    vec4 warp = NoiseAt(p * 0.7 + 0.53);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;

    // The nearest pillar's surface: the distance to it (negative inside), its outward normal and
    // how far along the pillar it is (0 at the base, 1 at the head)
    float distanceToSurface = 1e9;
    vec3 normal = vec3(0.0, 1.0, 0.0);
    float along = 0.0;
    int count = int(uNebula.bounds.w);
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        if (i >= count)
            break;
        vec3 closest;
        float h;
        float d = CloudDistance(i, q, closest, h);
        if (d < distanceToSurface)
        {
            distanceToSurface = d;
            normal = normalize(q - closest + vec3(0.0, 1e-4, 0.0));
            along = h;
        }
    }
    // Lumpy, with knots and globules standing off the surface
    vec4 n = NoiseAt(q);
    vec4 fine = NoiseAt(q * 2.7 + 3.1);
    distanceToSurface += (n.r - 0.5) * 2.0 * PILLAR_ROUGHNESS + (fine.g - 0.5) * PILLAR_ROUGHNESS;

    // Inside: dense, dark dust. The pillars rise from a cloud of it at the bottom.
    float floorHeight = -uNebula.shape.y * 0.85 + (warp.a - 0.5) * 0.3;
    float inside = smoothstep(0.02, -0.05, distanceToSurface);
    dust = PILLAR_DUST * max(inside, smoothstep(floorHeight + 0.05, floorHeight - 0.1, p.y) * extent);

    // The skin of the faces towards the stars, ionised and glowing (most at the heads), and the gas
    // evaporating off them towards the stars
    vec3 light = uNebula.light.xyz;
    float lit = smoothstep(-0.25, 0.7, dot(normal, light));
    float rim = exp(-pow(distanceToSurface / RIM_WIDTH, 2.0));
    float evaporating = exp(-max(distanceToSurface, 0.0) / EVAPORATION_LENGTH) * step(0.0, distanceToSurface);
    float headward = 0.35 + along;
    emission = lit * headward * (2.5 * rim * (uNebula.lines.z * SULPHUR_COLOUR * 0.6 + uNebula.lines.x * HYDROGEN_COLOUR * 0.8) +
                                 0.5 * evaporating * uNebula.lines.x * HYDROGEN_COLOUR) * (0.6 + 0.8 * fine.r);

    // The glowing gas of the cavity the stars have cleared: more oxygen nearer them, brighter on
    // its far side (behind the pillars) and thin in front of them, so they stand dark against it
    float towardsStars = dot(p, light) / uNebula.shape.y;
    float oxygen = smoothstep(-0.8, 0.9, towardsStars);
    float behind = (1.0 + BACK_GLOW * smoothstep(-0.1, -0.8, u.z)) * mix(1.0, 0.15, smoothstep(-0.1, 0.4, u.z));
    float gas = CAVITY_GLOW * (0.3 + 1.2 * n.b * n.b) * behind * extent * (1.0 - inside);
    emission += gas * (uNebula.lines.x * HYDROGEN_COLOUR * (1.0 - 0.5 * oxygen) + uNebula.lines.y * OXYGEN_COLOUR * oxygen);
}

void PlanetaryNebula(vec3 p, out vec3 emission, out float dust)
{
    float scale = max(uNebula.shape.x, max(uNebula.shape.y, uNebula.shape.z));
    vec4 warp = NoiseAt(p * 0.6 + 0.29);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;
    vec4 n = NoiseAt(q);
    vec4 fine = NoiseAt(q * 2.5 + 4.3);
    // Clumpy, and streaked with fine filaments
    float texture = (0.35 + 1.3 * n.b * n.b) * (0.55 + 0.9 * pow(Ridge(fine.r), 2.0));

    // The inner shell: its wall bright, the gas inside it dimmer. Doubly ionised oxygen glows
    // inside it, nearest the star, hydrogen in its wall and beyond.
    vec3 u = q / uNebula.shape.xyz;
    float r = length(u) + (warp.a - 0.5) * 0.12;
    float wall = exp(-pow((r - 1.0) / PLANETARY_WALL, 2.0));
    float filled = 0.15 * (1.0 - smoothstep(0.75, 1.0, r));
    float shell = (wall + filled) * texture;
    float oxygenShare = 1.0 - smoothstep(0.85, 1.6, r);
    emission = shell * IonisedGas(oxygenShare, 0.35);

    // The lobes blown out along its axes: thin walls, brighter towards their tips, ending in knots
    // of denser, cooler gas glowing in [N II]
    int count = int(uNebula.bounds.w);
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        if (i >= count)
            break;
        vec3 closest;
        float along;
        float d = CloudDistance(i, q, closest, along);
        float radius = mix(uNebula.cloudBase[i].w, uNebula.cloudHead[i].w, along);
        float width = LOBE_WALL * radius;
        float walls = exp(-pow(d / width, 2.0)) + 0.2 * smoothstep(0.0, -width, d);
        vec3 tip = q - uNebula.cloudHead[i].xyz;
        float knotSize = KNOT_SIZE * uNebula.cloudHead[i].w;
        float knot = exp(-dot(tip, tip) / (knotSize * knotSize)) * (0.5 + n.g);
        float lobe = uNebula.cloudInfo[i].x * walls * (0.4 + 0.6 * along) * texture;
        emission += lobe * IonisedGas(0.3 * (1.0 - along), 0.6) + 1.5 * uNebula.cloudInfo[i].x * knot * IonisedGas(0.0, 1.6);
        shell += lobe + knot;
    }

    // The faint halo of gas it threw off before
    float halo = HALO_GLOW * exp(-2.5 * max(length(u) - 1.0, 0.0)) * (1.0 - smoothstep(1.6, 2.2, length(u))) * (0.6 + 0.8 * n.a);
    emission += halo * IonisedGas(0.15, 0.3);

    emission *= PLANETARY_GLOW / scale;
    dust = PLANETARY_DUST * shell / scale;
}

void SupernovaShell(vec3 p, out vec3 emission, out float dust)
{
    vec3 u = p / uNebula.shape.xyz;
    float rho = length(u);
    emission = vec3(0.0);
    dust = 0.0;
    if (rho > 1.15 || rho < 0.55)
        return;
    float scale = max(uNebula.shape.x, max(uNebula.shape.y, uNebula.shape.z));

    // The shock's front: pushed in where it has met denser clouds and slowed (where it glows most),
    // and folded and crumpled on smaller scales
    vec4 large = NoiseAt(p * 0.22 + 3.7);
    vec4 warp = NoiseAt(p * 0.6 + 0.37);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;
    vec4 n = NoiseAt(q);
    vec4 fine = NoiseAt(q * 2.7 + 9.1);
    float slowed = smoothstep(0.45, 0.85, large.r);
    float front = 1.0 - SHELL_RIPPLE * slowed + SHELL_FOLDS * (2.0 * n.r - 1.0) + 0.15 * SHELL_FOLDS * (2.0 * fine.g - 1.0);

    // Behind the front (in widths of its sheet): [O III] glows right behind the shock, as the gas
    // is heated, and hydrogen a little further behind, as it cools
    float width = 1.0 / max(uNebula.form.w, 1.0);
    float x = (front - rho) / width;
    float oxygenLayer = exp(-pow((x - 0.5) / 0.6, 2.0));
    float hydrogenLayer = exp(-pow((x - 2.6) / 1.2, 2.0));

    // Bright along stretches of it: where the clouds it meets are dense, most on its east and west,
    // where it has reached the walls of the cavity the star's wind blew before it exploded
    float walls = 0.06 + 0.94 * smoothstep(0.45, 0.9, abs(u.x));
    float density = slowed * slowed * walls * (0.2 + 1.6 * n.b * n.b) * (0.5 + fine.r);
    float glow = SHELL_GLOW * uNebula.look.y * density / (width * scale);
    emission = glow * (uNebula.lines.y * OXYGEN_COLOUR * 1.2 * oxygenLayer +
                       (uNebula.lines.x * HYDROGEN_COLOUR + uNebula.lines.z * SULPHUR_COLOUR * 0.5) * hydrogenLayer);
    dust = 0.01 * density * hydrogenLayer / (width * scale);
}

void EmissionNebula(vec3 p, out vec3 emission, out float dust)
{
    float scale = max(uNebula.shape.x, max(uNebula.shape.y, uNebula.shape.z));
    vec3 u = p / uNebula.shape.xyz;
    vec4 warp = NoiseAt(p * 0.5 + 0.41);
    vec3 q = p + (warp.rgb - 0.5) * uNebula.form.z;
    // The structure, drawn out along the axes it streams along
    vec3 s = q / uNebula.stretch.xyz;
    vec4 n = NoiseAt(s);
    vec4 fine = NoiseAt(s * 2.3 + 7.1);
    vec4 large = NoiseAt(s * 0.35 + 2.7);
    // Its edge, ragged on large scales
    float extent = 1.0 - smoothstep(0.45, 1.05, length(u) + 0.9 * (large.g - 0.5));

    // The hot stars: how strongly the gas is ionised (and how hard the light is, for [O III]),
    // which way they are, and their light scattered off the dust near them
    float ionised = 0.0, oxygen = 0.0;
    vec3 towardsStars = vec3(0.0);
    vec3 starlight = vec3(0.0);
    int starCount = int(uNebula.bounds.y);
    for (int i = 0; i < MAX_STARS; i++)
    {
        if (i >= starCount)
            break;
        float radius = uNebula.starInfo[i].y;
        if (radius <= 0.0)
            continue;
        vec3 toStar = ToNebula(uNebula.stars[i].xyz) - q;
        float d = length(toStar);
        float strength = uNebula.starInfo[i].z;
        float w = strength * exp(-d / radius);
        ionised += w;
        oxygen += strength * exp(-d / (0.35 * radius));
        towardsStars += w * toStar / max(d, 1e-4);
        starlight += strength * Blackbody(uNebula.starInfo[i].x) * exp(-d / (0.3 * radius));
    }
    float oxygenShare = clamp(oxygen / max(ionised, 1e-4), 0.0, 1.0);
    vec3 lightDirection = towardsStars / max(length(towardsStars), 1e-6);

    // The dense clouds: their dust, and the nearest one's surface (roughened), for its rim
    float cloudDust = 0.0, inside = 0.0;
    float surface = 1e9, rimWidth = 1.0, rimDensity = 0.0, evaporationLength = 1.0;
    vec3 normal = vec3(0.0, 1.0, 0.0);
    int cloudCount = int(uNebula.bounds.w);
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        if (i >= cloudCount)
            break;
        vec3 closest;
        float along;
        float d = CloudDistance(i, q, closest, along);
        float radius = mix(uNebula.cloudBase[i].w, uNebula.cloudHead[i].w, along);
        // Roughened (only near its surface: further away, its roughness doesn't matter)
        float roughness = CLOUD_ROUGHNESS * radius;
        if (d < 1.5 * roughness)
        {
            vec4 rough = textureLod(uNoise, q / (CLOUD_NOISE_SCALE * radius) + 0.173 * float(i), 0.0);
            d += roughness * (1.6 * (2.0 * rough.r - 1.0) + 0.6 * (2.0 * rough.g - 1.0));
        }
        float width = max(CLOUD_RIM * radius, 2.5 * uNebula.light.w);
        float within = smoothstep(0.0, -width, d);
        cloudDust += uNebula.cloudInfo[i].x * CLOUD_DEPTH / radius * within;
        inside = max(inside, within * min(uNebula.cloudInfo[i].x, 1.0));
        if (d < surface)
        {
            surface = d;
            rimWidth = width;
            rimDensity = min(uNebula.cloudInfo[i].x, 1.0);
            evaporationLength = max(CLOUD_EVAPORATION * radius, width);
            normal = normalize(q - closest + vec3(0.0, 1e-5, 0.0));
        }
    }

    // The bubble: cleared inside, its gas swept up into a shell
    float cleared = 1.0, bubbleShell = 0.0;
    if (uNebula.bubble.w > 0.0)
    {
        float thickness = uNebula.bubbleShell.x;
        float r = length(q - uNebula.bubble.xyz) + (n.a - 0.5) * 1.5 * thickness;
        float x = (r - uNebula.bubble.w) / thickness;
        // Sharp at the cavity's edge, thinning outwards
        bubbleShell = exp(-x * x / (x < 0.0 ? 0.3 : 1.5));
        cleared = 1.0 - uNebula.bubbleShell.z * (1.0 - smoothstep(-1.5, 0.0, x));
    }

    // The gas: in clumps, or drawn out into wisps, carved into lanes and bays on large scales,
    // glowing where it is ionised
    float clumps = 0.05 + 2.4 * n.b * n.b * n.b;
    float wisps = 2.2 * pow(Ridge(n.r), uNebula.form.w) * (0.35 + 1.3 * fine.g);
    float structure = mix(clumps, wisps, clamp(uNebula.look.y, 0.0, 1.0)) * mix(0.1, 1.0, smoothstep(0.3, 0.75, large.b));
    float gas = structure * extent * cleared * (1.0 - inside);
    float glow = (1.0 - exp(-2.0 * ionised)) + 0.5 * ionised;
    emission = EMISSION_GLOW / scale * glow * gas * IonisedGas(oxygenShare, 0.35);

    // The bubble's shell, lit by the star inside it
    emission += EMISSION_GLOW / scale * uNebula.bubbleShell.y * bubbleShell * extent * (0.4 + 1.2 * n.g) * glow *
                IonisedGas(uNebula.bubbleShell.w, 0.2);

    // The clouds' rims facing the stars, ionised and glowing, and the gas evaporating off them
    float facing = rimDensity * smoothstep(0.0, 0.8, dot(normal, lightDirection));
    float rim = exp(-pow(surface / rimWidth, 2.0));
    float evaporating = exp(-max(surface, 0.0) / evaporationLength) * step(0.0, surface);
    emission += facing * glow * extent * (RIM_GLOW * rim / rimWidth + EVAPORATION_GLOW / scale * evaporating) * (0.5 + fine.r) *
                IonisedGas(0.3 * oxygenShare, 1.0);

    // Starlight scattered off the dust near the stars (bluer than the stars)
    float dusty = gas + inside;
    emission += uNebula.lines.w / scale * starlight * vec3(0.55, 0.8, 1.35) * dusty * (0.4 + n.g);

    dust = cloudDust + GAS_DUST / scale * structure * extent * smoothstep(0.35, 0.75, large.a);
}

void NebulaAt(vec3 p, out vec3 emission, out float dust)
{
    vec3 local = ToNebula(p);
    int kind = int(uNebula.shape.w + 0.5);
    if (kind == KIND_SUPERNOVA_REMNANT)
        SupernovaRemnant(local, emission, dust);
    else if (kind == KIND_HII_REGION)
        HIIRegion(local, emission, dust);
    else if (kind == KIND_PILLARS)
        Pillars(local, emission, dust);
    else if (kind == KIND_PLANETARY_NEBULA)
        PlanetaryNebula(local, emission, dust);
    else if (kind == KIND_SUPERNOVA_SHELL)
        SupernovaShell(local, emission, dust);
    else
        EmissionNebula(local, emission, dust);
    dust *= uNebula.look.x;
}

// Where a ray enters and leaves the bounding sphere (false if it misses)
bool ClipToBounds(vec3 from, vec3 direction, out float tNear, out float tFar)
{
    float radius = uNebula.bounds.x;
    float b = dot(from, direction);
    float c = dot(from, from) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant <= 0.0)
        return false;
    float root = sqrt(discriminant);
    tNear = max(-b - root, 0.0);
    tFar = -b + root;
    return tFar > tNear;
}

// The dust's transmittance between `from` and `to` (for the stars)
vec3 DustTransmittance(vec3 from, vec3 to, float dither)
{
    float depth = 0.0;
    float segment = distance(from, to);
    for (int i = 0; i < STAR_DUST_SAMPLES; i++)
    {
        vec3 emission;
        float dust;
        NebulaAt(mix(from, to, (float(i) + dither) / float(STAR_DUST_SAMPLES)), emission, dust);
        depth += dust;
    }
    return exp(-depth * segment / float(STAR_DUST_SAMPLES) * DUST_REDDENING);
}

void main()
{
    vec3 directionView = normalize(vec3(vNdc.x / uFrame.projection[0][0], vNdc.y / uFrame.projection[1][1], -1.0));
    vec3 direction = transpose(mat3(uFrame.view)) * directionView;
    vec3 from = uFrame.cameraPosition.xyz - uNebula.centre.xyz;
    float dither = Dither(gl_FragCoord.xy);

    // The gas
    vec3 light = vec3(0.0);
    vec3 transmittance = vec3(1.0);
    float tNear, tFar;
    if (ClipToBounds(from, direction, tNear, tFar))
    {
        int steps = int(uNebula.bounds.z);
        float dt = (tFar - tNear) / float(steps);
        for (int i = 0; i < MAX_STEPS; i++)
        {
            if (i >= steps)
                break;
            vec3 emission;
            float dust;
            NebulaAt(from + direction * (tNear + (float(i) + dither) * dt), emission, dust);
            vec3 depth = dust * dt * DUST_REDDENING;
            vec3 stepTransmittance = exp(-depth);
            // The light given off along the step, dimmed by the dust in front of it within the step
            vec3 selfShadow = mix(vec3(1.0), (1.0 - stepTransmittance) / max(depth, vec3(1e-6)), step(vec3(1e-4), depth));
            light += transmittance * emission * dt * selfShadow;
            transmittance *= stepTransmittance;
            if (max(transmittance.r, max(transmittance.g, transmittance.b)) < 1e-3)
                break;
        }
    }

    // The stars, as points about a pixel across
    float width = max(uNebula.look.w, 1e-6);
    int count = int(uNebula.bounds.y);
    for (int i = 0; i < MAX_STARS; i++)
    {
        if (i >= count)
            break;
        vec3 toStar = uNebula.stars[i].xyz - from;
        float along = dot(toStar, direction);
        if (along <= 0.0)
            continue;
        float angle = sqrt(max(dot(toStar, toStar) - along * along, 0.0)) / along;
        float profile = exp(-(angle * angle) / (width * width));
        if (profile < 1e-3)
            continue;
        vec3 star = uNebula.stars[i].w * Blackbody(uNebula.starInfo[i].x) * profile;
        light += star * DustTransmittance(from, uNebula.stars[i].xyz, dither);
    }

    // The sky behind
    light += transmittance * uNebula.look.z * StarField(direction, width);
    fragColour = vec4(light, 1.0);
}
