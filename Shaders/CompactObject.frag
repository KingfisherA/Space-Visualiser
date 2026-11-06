#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Procedural.glsl"

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
//   - a neutron star's beams, out from its magnetic poles, and its glowing magnetosphere
// and, where it ends, the neutron star's surface or the stars and glow of the sky (lensed: bent
// around the object).

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
    vec4 blackbody[BLACKBODY_SAMPLES]; // rgb = colour (luminance 1)
} uObject;

const int MAX_STEPS = 400;
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

// The neutron star: its hot spots' colour, its beams' colour (false colour: radio to X-rays), its
// magnetosphere's colour, and its field lines: drawn out to this many star radii from the
// magnetic axis (at the equator), spaced by this factor, this many around the axis, and this wide
// (star radii, at the star; wider further out)
const vec3 HOT_SPOT_COLOUR = vec3(0.75, 0.85, 1.0);
const vec3 BEAM_COLOUR = vec3(0.55, 0.6, 1.0);
const vec3 MAGNETOSPHERE_COLOUR = vec3(0.6, 0.45, 1.0);
const float FIELD_LINE_REACH = 6.0;
const float FIELD_LINE_SPACING = 1.6;
const float FIELD_LINES_AROUND = 6.0;
const float FIELD_LINE_WIDTH = 0.06;
// How much brighter the hot spots look when a beam sweeps past the camera
const float PULSE_GAIN = 6.0;

// The sky's faint blue glow
const vec3 SKY_COLOUR = vec3(0.0025, 0.0045, 0.011);
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

// The width of the neutron star's field lines at distance r
float FieldLineWidth(float r)
{
    return FIELD_LINE_WIDTH * uObject.star.x * (0.5 + 0.5 * r / uObject.star.x);
}

// The light a neutron star's beams and magnetosphere give off per unit length at p
vec3 PulsarEmission(vec3 p)
{
    float radius = uObject.star.x;
    float r = length(p);
    if (r < radius)
        return vec3(0.0);
    vec3 axis = uObject.magnetic.xyz;
    float c = dot(p, axis) / r; // cos(angle from the magnetic axis)
    vec3 light = vec3(0.0);

    // The beams: cones along the magnetic axis, both ways, fading out
    if (uObject.beams.x > 0.0)
    {
        float angle = acos(min(abs(c), 1.0));
        float across = angle / uObject.beams.y;
        float along = smoothstep(radius, 1.6 * radius, r) * exp(-(r - radius) / uObject.beams.z) * radius / r;
        light += BEAM_COLOUR * uObject.beams.x * exp(-across * across) * along * (1.0 + 2.0 * exp(-4.0 * across * across));
    }

    // The magnetosphere: glowing along some of the closed dipole field lines, r = L sin^2(angle
    // from the axis), at L spaced by FIELD_LINE_SPACING and at FIELD_LINES_AROUND angles around the axis
    if (uObject.beams.w > 0.0)
    {
        float sin2 = max(1.0 - c * c, 1e-4);
        float l = r / sin2 / radius;
        if (l > 1.2 && l < FIELD_LINE_REACH)
        {
            float shell = log(l) / log(FIELD_LINE_SPACING);
            float width = FieldLineWidth(r);
            // Distance to the nearest drawn shell, then to the nearest line on it (both in r_s)
            float shellDistance = abs(fract(shell + 0.5) - 0.5) * log(FIELD_LINE_SPACING) * r;
            vec3 helper = abs(axis.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            vec3 e1 = normalize(cross(axis, helper));
            vec3 e2 = cross(axis, e1);
            float around = atan(dot(p, e2), dot(p, e1)) * FIELD_LINES_AROUND / TAU;
            float aroundDistance = abs(fract(around + 0.5) - 0.5) * TAU / FIELD_LINES_AROUND * r * sqrt(sin2);
            float line = exp(-(shellDistance * shellDistance + aroundDistance * aroundDistance) / (width * width));
            light += MAGNETOSPHERE_COLOUR * uObject.beams.w * line * smoothstep(FIELD_LINE_REACH, 0.6 * FIELD_LINE_REACH, l) * radius / r;
        }
    }
    return light;
}

// The neutron star's surface where the ray (heading in `direction`) lands at normal n
vec3 NeutronStarSurface(vec3 n, vec3 direction)
{
    float radius = uObject.star.x;
    // Climbing out of the star's gravity reddens and dims its light
    float shift = mix(1.0, sqrt(1.0 - 1.0 / radius), uObject.discLook.y);
    float limb = 0.45 + 0.55 * max(dot(n, -direction), 0.0);
    vec3 light = uObject.star.y * pow(shift, 4.0) * limb * Blackbody(uObject.star.z * shift);

    // Hot spots at the magnetic poles, flashing as a beam (along the axis) sweeps past the light
    // leaving here towards the camera
    vec3 axis = uObject.magnetic.xyz;
    float polar = abs(dot(n, axis));
    float spot = smoothstep(uObject.star.w, mix(uObject.star.w, 1.0, 0.6), polar);
    float beamAngle = acos(min(abs(dot(axis, -direction)), 1.0)) / uObject.beams.y;
    float pulse = 1.0 + PULSE_GAIN * sign(uObject.beams.x) * exp(-beamAngle * beamAngle);
    light += spot * uObject.magnetic.w * pulse * pow(shift, 4.0) * HOT_SPOT_COLOUR;
    return light;
}

// The sky seen in a direction: stars and a faint glow
vec3 Sky(vec3 direction)
{
    return uObject.sky.y * SKY_COLOUR * (0.5 + Fbm(direction * 3.0)) + uObject.sky.x * StarField(direction, uObject.sky.z);
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
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float r = length(position);
        if (r < surface)
        {
            if (pulsar)
                light += transmittance * NeutronStarSurface(normalize(position), normalize(velocity));
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
        if (magnetosphere && r < FIELD_LINE_REACH * uObject.star.x)
            stepLength = min(stepLength, 0.5 * FieldLineWidth(r));
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
            light += transmittance * PulsarEmission(midpoint) * segment;
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
    // shadow they border
    if (escaped)
        light += transmittance * Sky(normalize(velocity));
    fragColour = vec4(light, 1.0);
}
