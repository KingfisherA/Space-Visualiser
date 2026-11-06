#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Universe.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 1) flat in vec3 vRadiance;
layout(location = 2) flat in vec4 vShape; // x = shape, y = seed, z = T-type, w = pass

// Added to the scene target (HDR)
layout(location = 0) out vec4 fragColour;

// Mirrors UniverseSprites.vert
const float DISC_SCALE = 0.27;
const float SPHEROID_EXTENT = 4.0;
const float TAU = 6.28318531;

// Emission lines' colours: hydrogen alpha (with nitrogen), doubly ionised oxygen, and the blue-white
// synchrotron light of a pulsar wind nebula
const vec3 H_ALPHA = vec3(1.0, 0.2, 0.3);
const vec3 OXYGEN = vec3(0.25, 0.95, 0.85);
const vec3 SYNCHROTRON = vec3(0.55, 0.68, 1.0);
// The blue of young stars in galaxies' arms and irregulars
const vec3 YOUNG_STARS = vec3(0.68, 0.82, 1.0);

vec4 NoiseAt(vec2 uv, float seed, float scale)
{
    return textureLod(uNoise, vec3(uv * scale + seed * vec2(7.31, 3.17), seed * 5.73), 0.0);
}

// Galaxy discs (uv in radii, in the disc's plane)
vec3 Disc(vec2 uv, int shape, float seed, float typeCode)
{
    float r = length(uv);
    float disc = exp(-r / DISC_SCALE) * (1.0 - smoothstep(0.9, 1.25, r));
    vec4 n = NoiseAt(uv, seed, 1.3);
    vec4 fine = NoiseAt(uv, seed + 0.37, 3.7);
    if (shape == SHAPE_IRREGULAR)
    {
        // Clumps of young stars, scattered off centre
        float clumps = 0.25 + 2.4 * n.r * n.r * (0.5 + fine.g);
        return mix(vec3(1.0), YOUNG_STARS, 0.5) * disc * clumps;
    }

    // Two trailing logarithmic arms (wound tighter in earlier types), with a bar in barred spirals
    float angle = atan(uv.y, uv.x) + seed * TAU;
    float arms = smoothstep(0.5, 2.5, typeCode) * (1.0 - smoothstep(7.5, 9.5, typeCode));
    float pitch = radians(mix(11.0, 26.0, clamp((typeCode - 1.0) / 6.0, 0.0, 1.0)));
    // (Wobbled by the noise, so they aren't perfect spirals)
    float phase = 2.0 * (angle - log(max(r, 0.03)) / tan(pitch)) + 1.5 * (n.a - 0.5);
    float arm = pow(0.5 + 0.5 * cos(phase), 6.0);
    float mask = smoothstep(0.08, 0.3, r) * arms;
    // Knots of young stars along the arms, and (in later types) flocculent fragments between them
    float knots = 0.35 + 1.3 * fine.r * fine.r;
    float flocculent = smoothstep(3.0, 7.0, typeCode) * 0.6 * fine.g * fine.b;
    float pattern = mix(1.0, 0.45 + 3.2 * arm * knots + flocculent, mask);
    // Dust along the arms' inner edges
    float dust = pow(0.5 + 0.5 * cos(phase + 0.7), 6.0) * mask * smoothstep(0.35, 0.7, n.g);
    pattern *= 1.0 - 0.55 * dust;
    vec3 colour = mix(vec3(1.0), YOUNG_STARS, clamp(arm * mask * 1.2, 0.0, 0.8));
    vec3 light = colour * disc * pattern * mix(vec3(1.0), vec3(1.0, 0.85, 0.7), dust);
    if (shape == SHAPE_BARRED_SPIRAL)
    {
        // Old stars: a little warmer than the disc. The arms leave its ends.
        vec2 b = vec2(cos(seed * TAU) * uv.x + sin(seed * TAU) * uv.y, -sin(seed * TAU) * uv.x + cos(seed * TAU) * uv.y);
        light += vec3(1.0, 0.9, 0.78) * 0.8 * exp(-(b.x * b.x) / (0.28 * 0.28) - (b.y * b.y) / (0.06 * 0.06));
    }
    return light;
}

// Spheroids and bulges (uv in core radii)
float Spheroid(vec2 uv)
{
    float r2 = dot(uv, uv);
    float profile = 1.0 / ((1.0 + r2) * (1.0 + r2));
    return profile * (1.0 - smoothstep(0.7, 1.0, sqrt(r2) / SPHEROID_EXTENT));
}

// Nebulae (uv in radii): each averages about 1 inside its radius
vec3 Nebula(vec2 uv, int shape, float seed)
{
    float r = length(uv);
    vec4 n = NoiseAt(uv, seed, 0.9);
    vec4 fine = NoiseAt(uv, seed + 0.53, 2.6);
    float edge = 1.0 - smoothstep(0.95, 1.15, r);
    if (shape == SHAPE_PLANETARY_NEBULA)
    {
        // A bright shell, oxygen inside and hydrogen and nitrogen at its rim
        float shell = exp(-pow((r - 0.62) / 0.18, 2.0)) * (0.6 + 0.8 * fine.r);
        float fill = 0.4 * exp(-r * r * 3.0);
        vec3 colour = mix(OXYGEN, H_ALPHA, smoothstep(0.45, 0.85, r));
        return colour * (shell + fill) * 2.2 * edge;
    }
    if (shape == SHAPE_HII_REGION)
    {
        // Glowing hydrogen in clumps, cut by dark dust, with an oxygen glow around its hot stars
        float cloud = exp(-r * r * 2.2) * (0.15 + 2.6 * n.r * n.r * (0.4 + fine.g));
        float dust = smoothstep(0.5, 0.78, n.b);
        vec3 colour = mix(H_ALPHA, OXYGEN, 0.45 * exp(-r * r * 7.0) * fine.a);
        return colour * cloud * (1.0 - 0.8 * dust) * 2.5 * edge;
    }
    // Supernova remnants: filaments (ridges of the noise) in an expanding shell
    float ridge = 1.0 - abs(2.0 * fine.r - 1.0);
    float filaments = pow(ridge, 5.0);
    if (shape == SHAPE_FILLED_REMNANT)
    {
        // A pulsar wind nebula's synchrotron glow, in a cage of filaments
        float glow = exp(-r * r * 3.0) * (0.7 + 0.5 * n.r);
        float cage = filaments * smoothstep(0.2, 0.8, r) * (1.0 - smoothstep(0.85, 1.05, r));
        return (SYNCHROTRON * glow * 1.5 + vec3(1.0, 0.45, 0.25) * cage * 6.0) * edge;
    }
    float shell = exp(-pow((r - 0.85) / 0.13, 2.0)) * (0.25 + 3.0 * filaments);
    vec3 colour = mix(H_ALPHA, OXYGEN, smoothstep(0.35, 0.7, n.g));
    return colour * (shell * 3.0 + 0.1 * exp(-r * r * 2.0)) * edge;
}

void main()
{
    int pass = int(vShape.w + 0.5);
    int shape = int(vShape.x + 0.5);
    vec3 light;
    if (pass == PASS_GALAXY_POINTS || pass == PASS_NEBULA_POINTS)
    {
        // A Gaussian (as the stars'), lowered to reach 0 at the quad's inscribed circle
        float r2 = dot(vUv, vUv);
        float edge = exp(-0.5 * POINT_EXTENT * POINT_EXTENT);
        light = vRadiance * max(exp(-0.5 * r2) - edge, 0.0) / (1.0 - edge);
    }
    else if (pass == PASS_GALAXY_DISCS)
    {
        light = vRadiance * Disc(vUv, shape, vShape.y, vShape.z);
    }
    else if (pass == PASS_GALAXY_SPHEROIDS)
    {
        light = vRadiance * Spheroid(vUv);
    }
    else
    {
        light = vRadiance * Nebula(vUv, shape, vShape.y);
    }
    fragColour = vec4(max(light, vec3(0.0)), 0.0);
}
