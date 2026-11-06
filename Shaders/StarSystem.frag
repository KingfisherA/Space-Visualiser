#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "Procedural.glsl"

layout(location = 0) in vec2 vNdc;

// Radiance (HDR)
layout(location = 0) out vec4 fragColour;

// The star test's bodies: each pixel's ray is traced to the nearest body (an ellipsoid: a sphere
// stretched along the line to its partner by their tides) and its surface shaded. Lengths are in
// solar radii. Surface brightness is relative to the Sun's, as a blackbody's: (T / 5772 K)^4.
//
// Around each body is its glow, and a body smaller than a pixel or two is drawn as a point of the
// same total light (so distant stars don't flicker or vanish). Behind them all are the sky's stars.

const int MAX_BODIES = 4; // Mirrors MAX_STELLAR_BODIES in src/SV/Universe/StarSystem.hpp

// Mirrors StarSystemUniforms in src/SV/Universe/StarSystem.cpp (std140)
layout(std140, set = 1, binding = 0) uniform StarSystemUniforms
{
    vec4 info; // x = body count, y = pixel angle (radians), z = star field brightness, w = surface detail
    vec4 position[MAX_BODIES]; // xyz = centre (render coordinates), w = radius
    vec4 surface[MAX_BODIES];  // x = temperature (K), y = kind (0 star, 1 brown dwarf, 2 white dwarf), z = granulation, w = spots
    vec4 stretch[MAX_BODIES];  // xyz = the tides' axis, w = stretch along it
    vec4 look[MAX_BODIES];     // x = limb darkening, y = glow size, z = glow strength, w = rotation (radians)
    vec4 tint[MAX_BODIES];     // rgb = tint
    vec4 flare[MAX_BODIES];    // xyz = where (in the turning frame), w = brightness
    vec4 display; // x = point star gain
    vec4 blackbody[BLACKBODY_SAMPLES]; // rgb = colour (luminance 1)
} uSystem;

// Four independent channels of tileable fBm, evenly spread over [0, 1]
layout(set = 1, binding = 1) uniform sampler3D uNoise;

const float SUN_TEMPERATURE = 5772.0;
// Spots are this much cooler than the surface around them
const float SPOT_COOLING = 0.72;
// Light from a companion reflected (or absorbed and given off again) by a surface
const float IRRADIATION_ALBEDO = 0.5;
// A flare's size (radians across the surface), temperature (K), and the fraction of its patch
// glowing at that temperature
const float FLARE_SIZE = 0.12;
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

// The light leaving a surface at a temperature, relative to the Sun's
vec3 SurfaceLight(float temperature)
{
    float relative = temperature / SUN_TEMPERATURE;
    return Blackbody(temperature) * relative * relative * relative * relative;
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

// The light from body i's surface at `hit`, seen along `direction`
vec3 ShadeSurface(int i, vec3 hit, vec3 direction)
{
    vec3 local = hit - uSystem.position[i].xyz;
    // The ellipsoid's normal: the gradient of |Unstretch(p)|^2
    vec3 normal = normalize(Unstretch(Unstretch(local, i), i));
    vec3 turning = ToTurningFrame(normalize(local), i);
    float mu = max(dot(normal, -direction), 0.0);
    float limb = 1.0 - uSystem.look[i].x * (1.0 - sqrt(mu));
    float detail = uSystem.info.w;
    float temperature = uSystem.surface[i].x;
    int kind = int(uSystem.surface[i].y + 0.5);
    float pattern = 1.0;

    if (kind == 0)
    {
        // Convection cells (hot rising centres, cooler edges), and spots at low and middle latitudes
        float cells = uSystem.surface[i].z;
        vec4 n = NoiseAt(turning * cells / 8.0);
        vec4 m = NoiseAt(turning * cells / 3.0 + 3.7);
        pattern = mix(1.0, 0.75 + 0.35 * n.r + 0.15 * (m.g - 0.5), detail);
        float spots = uSystem.surface[i].w * detail;
        if (spots > 0.0)
        {
            // (The noise is spread evenly, so the threshold sets the fraction covered)
            float spotNoise = NoiseAt(turning * 2.2 + 7.1).b;
            float threshold = 1.0 - 0.3 * spots;
            float spot = smoothstep(threshold, threshold + 0.04, spotNoise) * (1.0 - smoothstep(0.55, 0.75, abs(turning.y)));
            temperature *= mix(1.0, SPOT_COOLING, spot);
        }
        // Gravity darkening: the stretched ends (with weaker gravity) are cooler
        float stretch = uSystem.stretch[i].w;
        float along = dot(normalize(local), uSystem.stretch[i].xyz);
        temperature *= 1.0 - 0.8 * (stretch - 1.0) * along * along;
    }
    else if (kind == 1)
    {
        // Bands of cloud, rippled, with gaps glowing from the hotter layers below
        vec4 warp = NoiseAt(turning * 0.8 + 1.3);
        float latitude = turning.y + 0.12 * (warp.r - 0.5);
        float bands = 0.5 + 0.5 * sin(latitude * BROWN_DWARF_BANDS * 3.14159);
        vec4 clouds = NoiseAt(vec3(turning.x * 0.6, turning.y * 2.5, turning.z * 0.6) + warp.gba * 0.3);
        pattern = mix(1.0, mix(0.55, 1.35, 0.6 * bands + 0.4 * clouds.g), detail);
        temperature *= mix(1.0, 0.93 + 0.1 * clouds.b, detail);
    }

    vec3 light = SurfaceLight(temperature) * uSystem.tint[i].rgb * limb * pattern;

    // A flare: a small, hot, bright patch
    if (uSystem.flare[i].w > 0.0)
    {
        float angle = acos(clamp(dot(turning, normalize(uSystem.flare[i].xyz)), -1.0, 1.0)) / FLARE_SIZE;
        light += uSystem.flare[i].w * exp(-angle * angle) * FLARE_SHARE * SurfaceLight(FLARE_TEMPERATURE);
    }

    // The light of the other bodies falling on it (the reflection effect in close binaries)
    int count = int(uSystem.info.x);
    for (int j = 0; j < MAX_BODIES; j++)
    {
        if (j >= count || j == i)
            continue;
        vec3 toOther = uSystem.position[j].xyz - hit;
        float distance = length(toOther);
        float facing = max(dot(normal, toOther / distance), 0.0);
        float size = uSystem.position[j].w / distance;
        light += IRRADIATION_ALBEDO * size * size * facing * SurfaceLight(uSystem.surface[j].x) * uSystem.tint[j].rgb;
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
        light = ShadeSurface(hitBody, from + direction * nearest, direction);

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
        vec3 surfaceLight = SurfaceLight(uSystem.surface[i].x) * uSystem.tint[i].rgb;

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

        // The glow beyond its edge
        if (impact > radius && uSystem.look[i].z > 0.0)
        {
            float height = (impact - radius) / (uSystem.look[i].y * radius);
            light += discShown * uSystem.look[i].z * surfaceLight * exp(-height) / (1.0 + 4.0 * height);
        }
    }

    if (hitBody < 0)
        light += uSystem.info.z * StarField(direction, pixel);
    fragColour = vec4(light, 1.0);
}
