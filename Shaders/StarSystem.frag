#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Procedural.glsl"
#include "Photosphere.glsl"
#include "StylisedStar.glsl"

layout(location = 0) in vec2 vNdc;

// Radiance (HDR)
layout(location = 0) out vec4 fragColour;

// The star test's bodies: each pixel's ray is traced to the nearest body (an ellipsoid: a sphere
// stretched along the line to its partner by their tides) and its surface shaded. Lengths are in
// solar radii. Light is relative to the Sun's surface in visible light: a blackbody's luminance at
// the body's temperature (BlackbodyLuminance in src/SV/Universe/Star.cpp).
//
// Stars' surfaces are their photospheres (Photosphere.glsl: limb darkening from the temperature,
// granulation, spots and faculae), cooled where their partner's tides stretch them (gravity
// darkening) and heated where their partner's light falls on them (the reflection effect).
//
// Around each body is its glow, and a body smaller than a pixel or two is drawn as a point of the
// same total light (so distant stars don't flicker or vanish). Behind them all are the sky's stars.
//
// Or the stars (not brown dwarfs) are stylised, as the solar system draws the Sun (StylisedStar.glsl):
// their surfaces and coronas in display colours, turned into the radiance that the compositor shows
// as those colours.

const int MAX_BODIES = 4; // Mirrors MAX_STELLAR_BODIES in src/SV/Universe/StarSystem.hpp
// The stylised coronas' extent (stellar radii), as Shaders/Include/CoronaPushConstants.glsl's
const float CORONA_EXTENT = 4.0;
const float CORONA_BLOOM_COMPENSATION = 0.7;

// Mirrors StarSystemUniforms in src/SV/Universe/StarSystem.cpp (std140)
layout(std140, set = 1, binding = 0) uniform StarSystemUniforms
{
    vec4 info; // x = body count, y = pixel angle (radians), z = star field brightness, w = surface detail
    vec4 position[MAX_BODIES]; // xyz = centre (render coordinates), w = radius
    vec4 surface[MAX_BODIES];  // x = temperature (K), y = kind (0 star, 1 brown dwarf, 2 white dwarf), z = granules per radius, w = spots
    vec4 stretch[MAX_BODIES];  // xyz = the tides' axis, w = stretch along it
    vec4 look[MAX_BODIES];     // x = granules' age (lifetimes), y = glow size, z = glow strength, w = rotation (radians)
    vec4 tint[MAX_BODIES];     // rgb = tint, w = surface luminance (relative to the Sun's)
    vec4 flare[MAX_BODIES];    // xyz = where (in the turning frame), w = brightness
    vec4 display; // x = point star gain, y = 1 for stylised stars, z = 1 / exposure
    vec4 blackbody[BLACKBODY_SAMPLES]; // rgb = colour (luminance 1)
} uSystem;

// Four independent channels of tileable fBm, evenly spread over [0, 1]
layout(set = 1, binding = 1) uniform sampler3D uNoise;

// Gravity darkening (von Zeipel 1924): temperature goes as surface gravity to this power, in stars
// with radiative envelopes (hotter than about 7000 K) and with convective ones (Lucy 1967)
const float RADIATIVE_GRAVITY_DARKENING = 0.25;
const float CONVECTIVE_GRAVITY_DARKENING = 0.08;
// The share of a companion's light falling on a surface that heats it (the rest is reflected):
// convective envelopes give about half back at once, radiative ones absorb it all
const float CONVECTIVE_HEATING = 0.5;
// A flare's size (radians across the surface), temperature (K), and the fraction of its patch
// glowing at that temperature
const float BODY_FLARE_SIZE = 0.12;
const float FLARE_TEMPERATURE = 9000.0;
const float FLARE_SHARE = 0.05;
// A brown dwarf's cloud bands: how many from pole to pole
const float BROWN_DWARF_BANDS = 7.0;

vec3 Blackbody(float temperature)
{
    float x = BlackbodyIndex(temperature);
    int i = min(int(x), BLACKBODY_SAMPLES - 2);
    return mix(uSystem.blackbody[i].rgb, uSystem.blackbody[i + 1].rgb, x - float(i));
}

vec3 StarColour(float temperature)
{
    return Blackbody(temperature);
}

// Whether body i is drawn stylised
bool IsStylised(int i)
{
    return uSystem.display.y > 0.5 && int(uSystem.surface[i].y + 0.5) != 1;
}

// The radiance the compositor shows as a display colour (before bloom)
vec3 FromDisplay(vec3 colour)
{
    vec3 linear = min(pow(max(colour, vec3(0.0)), vec3(2.2)), vec3(0.999));
    return -log(vec3(1.0) - linear) * uSystem.display.z;
}

// Body i's stylised colour, from its temperature
vec3 StylisedBodyColour(int i)
{
    return StylisedColour(Blackbody(uSystem.surface[i].x) * uSystem.tint[i].rgb, Blackbody(5772.0));
}

// The light of body i's surface on average (a blackbody's at its temperature)
vec3 BodyLight(int i)
{
    return Blackbody(uSystem.surface[i].x) * uSystem.tint[i].rgb * uSystem.tint[i].w;
}

vec4 NoiseAt(vec3 p)
{
    return textureLod(uNoise, p, 0.0);
}

// Takes a vector into the body's stretched frame, where it is a sphere: shortened along the tides'
// axis by the stretch, and lengthened across it (so its volume is kept)
vec3 Unstretch(vec3 v, int i)
{
    vec3 axis = uSystem.stretch[i].xyz;
    float stretch = uSystem.stretch[i].w;
    float along = dot(v, axis);
    return (v - along * axis) * sqrt(stretch) + axis * (along / stretch);
}

// Where the ray first meets body i (or -1)
float Intersect(vec3 from, vec3 direction, int i)
{
    vec3 o = Unstretch(from - uSystem.position[i].xyz, i);
    vec3 d = Unstretch(direction, i);
    float radius = uSystem.position[i].w;
    float a = dot(d, d);
    float b = dot(o, d);
    float c = dot(o, o) - radius * radius;
    float discriminant = b * b - a * c;
    if (discriminant < 0.0)
        return -1.0;
    float t = (-b - sqrt(discriminant)) / a;
    return t > 0.0 ? t : -1.0;
}

// Turns a direction from the body's centre into its turning frame (spinning about y)
vec3 ToTurningFrame(vec3 n, int i)
{
    float angle = uSystem.look[i].w;
    float c = cos(angle), s = sin(angle);
    return vec3(c * n.x - s * n.z, n.y, s * n.x + c * n.z);
}

// The spots of the body being shaded (its surface.w), for SpotActivity
float spotCoverage = 0.0;

float SpotActivity(vec3 site)
{
    // Sun-like stars have their spots in belts either side of the equator; heavily spotted ones
    // (young, fast turning and red dwarfs) at every latitude, up to the poles
    float latitude = abs(site.y);
    float belts = smoothstep(0.05, 0.15, latitude) * (1.0 - smoothstep(0.5, 0.65, latitude));
    float where = mix(belts, 1.0, smoothstep(0.15, 0.4, spotCoverage));
    // (The noise is spread evenly, so the threshold sets the share of sites with spots)
    float threshold = 1.0 - 0.6 * spotCoverage;
    return smoothstep(threshold, threshold + 0.06, NoiseAt(site * 0.7 + 7.1).b) * where;
}

// The light from body i's surface at `hit` (`distance` along the ray), seen along `direction`
vec3 ShadeSurface(int i, vec3 hit, vec3 direction, float distance)
{
    vec3 local = hit - uSystem.position[i].xyz;
    float radius = uSystem.position[i].w;
    // The ellipsoid's normal: along the gradient of |Unstretch(p)|^2
    vec3 gradient = Unstretch(Unstretch(local, i), i);
    vec3 normal = normalize(gradient);
    vec3 turning = ToTurningFrame(normalize(local), i);
    float mu = max(dot(normal, -direction), 0.0);
    float detail = uSystem.info.w;
    float temperature = uSystem.surface[i].x;
    int kind = int(uSystem.surface[i].y + 0.5);
    // A pixel on the unit sphere, foreshortened towards the limb
    float footprint = uSystem.info.y * distance / radius / max(mu, 0.05);
    float radiative = smoothstep(6500.0, 7500.0, temperature);
    float pattern = 1.0;

    if (IsStylised(i))
        return FromDisplay(StylisedSurface(turning, mu, StylisedBodyColour(i), uFrame.times.y, uSystem.info.y * distance / radius));

    // Gravity darkening: tides stretch the star, and its surface gravity (the gradient of the
    // potential the ellipsoid stands for, relative to a sphere's) is weaker at the stretched ends
    float gravity = length(gradient) / radius;
    float T = temperature * pow(gravity, mix(CONVECTIVE_GRAVITY_DARKENING, RADIATIVE_GRAVITY_DARKENING, radiative));

    // The light of the other bodies falling on it heats it: in a close binary, the face of the cooler
    // star turned to the hotter glows hotter (the reflection effect). T^4 grows by the flux absorbed.
    float heating = 0.0;
    int count = int(uSystem.info.x);
    for (int j = 0; j < MAX_BODIES; j++)
    {
        if (j >= count || j == i)
            continue;
        vec3 toOther = uSystem.position[j].xyz - hit;
        float separation = length(toOther);
        float facing = max(dot(normal, toOther / separation), 0.0);
        float size = uSystem.position[j].w / separation;
        float ratio = uSystem.surface[j].x / temperature;
        heating += size * size * facing * ratio * ratio * ratio * ratio;
    }
    T *= pow(1.0 + mix(CONVECTIVE_HEATING, 1.0, radiative) * heating, 0.25);

    if (kind == 0)
    {
        // Granulation (its contrast falls towards the limb, where the granules are seen side on)
        float strength = GranulationStrength(temperature) * detail;
        if (strength > 0.0)
            T *= 1.0 + GRANULATION_CONTRAST * strength * Granulation(turning, uSystem.surface[i].z, uSystem.look[i].x, footprint) * smoothstep(0.0, 0.3, mu);
        // Spots and faculae
        spotCoverage = uSystem.surface[i].w;
        if (spotCoverage > 0.0 && detail > 0.0)
        {
            vec2 spots = Starspots(turning, temperature, footprint, detail);
            T *= spots.x * (1.0 + FaculaWarming(spots.y, mu));
        }
    }
    else if (kind == 1)
    {
        // Bands of cloud, rippled, with gaps glowing from the hotter layers below
        vec4 warp = NoiseAt(turning * 0.8 + 1.3);
        float latitude = turning.y + 0.12 * (warp.r - 0.5);
        float bands = 0.5 + 0.5 * sin(latitude * BROWN_DWARF_BANDS * 3.14159);
        vec4 clouds = NoiseAt(vec3(turning.x * 0.6, turning.y * 2.5, turning.z * 0.6) + warp.gba * 0.3);
        pattern = mix(1.0, mix(0.55, 1.35, 0.6 * bands + 0.4 * clouds.g), detail);
        T *= mix(1.0, 0.97 + 0.04 * clouds.b, detail);
    }

    vec3 light = BodyLight(i) * PhotosphereLight(temperature, T, mu) * pattern;

    // A flare: a small, hot, bright patch
    if (uSystem.flare[i].w > 0.0)
    {
        float angle = acos(clamp(dot(turning, normalize(uSystem.flare[i].xyz)), -1.0, 1.0)) / BODY_FLARE_SIZE;
        light += uSystem.flare[i].w * exp(-angle * angle) * FLARE_SHARE * BodyLight(i) * PlanckRatio(FLARE_TEMPERATURE, temperature);
    }
    return light;
}

void main()
{
    vec3 directionView = normalize(vec3(vNdc.x / uFrame.projection[0][0], vNdc.y / uFrame.projection[1][1], -1.0));
    vec3 direction = transpose(mat3(uFrame.view)) * directionView;
    vec3 from = uFrame.cameraPosition.xyz;
    int count = int(uSystem.info.x);
    float pixel = max(uSystem.info.y, 1e-7);

    // The nearest body on the ray
    float nearest = 1e30;
    int hitBody = -1;
    for (int i = 0; i < MAX_BODIES; i++)
    {
        if (i >= count)
            break;
        float t = Intersect(from, direction, i);
        if (t > 0.0 && t < nearest)
        {
            nearest = t;
            hitBody = i;
        }
    }

    vec3 light = vec3(0.0);
    if (hitBody >= 0)
        light = ShadeSurface(hitBody, from + direction * nearest, direction, nearest);

    // Glows, and bodies too small to see as discs as points (not those behind the body hit)
    for (int i = 0; i < MAX_BODIES; i++)
    {
        if (i >= count)
            break;
        vec3 toCentre = uSystem.position[i].xyz - from;
        float along = dot(toCentre, direction);
        if (along <= 0.0)
            continue;
        float radius = uSystem.position[i].w;
        if (hitBody >= 0 && hitBody != i && along > nearest + radius)
            continue;
        float impact = length(toCentre - along * direction);
        float angularRadius = radius / length(toCentre);
        float angle = impact / along;
        vec3 surfaceLight = BodyLight(i);

        // As a point: a Gaussian about a pixel across with the disc's light, fading out as the disc
        // grows. A distant star's light is spread over far less than a pixel, so it would be lost at
        // an exposure for stars' surfaces: it is raised by the point gain times its square root (so
        // faint stars are raised more), as stars are in photographs exposed for them. The gain fades
        // as the star nears a pixel across, so it doesn't jump as it becomes a disc.
        float discShown = smoothstep(0.7, 2.0, angularRadius / pixel);
        float spread = angularRadius / pixel;
        vec3 pointLight = surfaceLight * spread * spread;
        float luminance = max(dot(pointLight, vec3(0.2126, 0.7152, 0.0722)), 1e-12);
        float gain = uSystem.display.x * (1.0 - smoothstep(0.1, 0.7, spread));
        pointLight += gain * pointLight * inversesqrt(luminance);
        // Brighter points are drawn larger, as they are in photographs
        float width = pixel * (1.0 + 0.5 * log(1.0 + gain * sqrt(luminance)));
        light += (1.0 - discShown) * pointLight * exp(-(angle / width) * (angle / width));

        // The stylised corona and hydrogen emission beyond its edge
        if (impact > radius && IsStylised(i) && discShown > 0.0)
        {
            vec3 offset = -(toCentre - along * direction);
            float b = impact / radius;
            if (b < CORONA_EXTENT)
            {
                mat3 toModel = mat3(ToTurningFrame(vec3(1.0, 0.0, 0.0), i), ToTurningFrame(vec3(0.0, 1.0, 0.0), i),
                                    ToTurningFrame(vec3(0.0, 0.0, 1.0), i));
                float fade = 1.0 - smoothstep(0.5 * CORONA_EXTENT, 0.95 * CORONA_EXTENT, b);
                vec3 corona = StylisedCorona(offset / impact, direction, toModel, b, StylisedBodyColour(i), 1.0, uFrame.times.y,
                                             pixel * along / radius);
                // (Dimmed a little: the compositor's bloom spreads it further, unlike the solar system's)
                light += discShown * fade * CORONA_BLOOM_COMPENSATION * FromDisplay(corona);
            }
        }
        // The glow beyond its edge
        else if (impact > radius && uSystem.look[i].z > 0.0)
        {
            float height = (impact - radius) / (uSystem.look[i].y * radius);
            light += discShown * uSystem.look[i].z * surfaceLight * exp(-height) / (1.0 + 4.0 * height);
        }
    }

    if (hitBody < 0)
        light += uSystem.info.z * StarField(direction, pixel);
    fragColour = vec4(light, 1.0);
}
