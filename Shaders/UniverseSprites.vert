#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Universe.glsl"

layout(location = 0) in vec2 aCorner;  // The quad's corner, in [-1, 1]
layout(location = 1) in vec4 aHigh;    // xyz = position (high part), w = radius (pc)
layout(location = 2) in vec4 aLow;     // xyz = position (low part), w = shape (SHAPE_*)
layout(location = 3) in vec4 aMajor;   // xyz = the major axis (unit), w = axis ratio
layout(location = 4) in vec4 aNormal;  // xyz = the disc's normal (unit), w = T-type (galaxies)
layout(location = 5) in vec4 aColour;  // rgb = colour (luminance 1), w = luminosity (solar)
layout(location = 6) in vec4 aExtra;   // x = seed in [0, 1), y = the bulge's share of the light (galaxies)

layout(location = 0) out vec2 vUv;
layout(location = 1) flat out vec3 vRadiance; // Colour times the light per pixel at the profile's unit
layout(location = 2) flat out vec4 vShape;    // x = shape, y = seed, z = T-type, w = pass

// Sprites are drawn this far out, in radii (galaxy discs and nebulae) or core radii (spheroids)
const float DISC_EXTENT = 1.25;
const float NEBULA_EXTENT = 1.15;
const float SPHEROID_EXTENT = 4.0;
// Discs' exponential scale length, and spheroids' and bulges' core radii, as fractions of the radius
const float DISC_SCALE = 0.27;
const float ELLIPTICAL_CORE = 0.18;
const float BULGE_CORE = 0.1;
// Sprites fade in from points as they grow from this many pixels across (as a radius) to the next
const float RESOLVE_START = 1.0;
const float RESOLVE_END = 3.0;
// Points fainter than this (in reference fluxes, after the exposure) aren't drawn
const float MIN_LIGHT = 2e-4;
const float MIN_MARKER_DIM = 0.04;

vec3 NebulaColour(int shape)
{
    if (shape == SHAPE_PLANETARY_NEBULA)
        return vec3(0.35, 1.0, 0.85);
    if (shape == SHAPE_HII_REGION)
        return vec3(1.0, 0.35, 0.45);
    return vec3(1.0, 0.6, 0.45);
}

void main()
{
    int shape = int(aLow.w + 0.5);
    int pass = int(uPass.pass.x + 0.5);
    bool nebula = shape >= SHAPE_PLANETARY_NEBULA;
    vec3 relative = FromCamera(aHigh.xyz, aLow.xyz);
    float distance = max(length(relative), 1e-9);
    vec3 viewDirection = relative / distance;
    float radius = aHigh.w;
    float pixel = uUniverse.viewport.x;
    float radiusPixels = radius / distance / pixel;
    float resolved = smoothstep(RESOLVE_START, RESOLVE_END, radiusPixels);
    float shown = nebula ? ShownNebula(aHigh.xyz) : Shown(aHigh.xyz, uUniverse.hiddenGalaxy) * Shown(aHigh.xyz, uUniverse.hiddenMilkyWay);
    float surfaceGain = (nebula ? uUniverse.extended.z : uUniverse.extended.x) * shown;
    float luminosity = aColour.w;
    float bulge = aExtra.y;
    vShape = vec4(float(shape), aExtra.x, aNormal.w, float(pass));
    gl_Position = CULLED;
    vRadiance = vec3(0.0);
    vUv = vec2(0.0);

    if (pass == PASS_GALAXY_POINTS || pass == PASS_NEBULA_POINTS)
    {
        // Too small to see as a sprite: its light as a point, raised (for galaxies, as the camera
        // zooms out; for nebulae, as markers when the galaxy is in view) until it is resolved
        if (resolved >= 1.0)
            return;
        float gain = nebula ? 1.0 : mix(uUniverse.extended.y, 1.0, smoothstep(0.3, RESOLVE_START, radiusPixels));
        float light = luminosity / (distance * distance) / REFERENCE_FLUX * surfaceGain * gain;
        vec3 colour = nebula ? NebulaColour(shape) : aColour.rgb;
        if (nebula)
        {
            float zoom = uUniverse.stars.z;
            float dim = clamp(zoom * zoom / (distance * distance), MIN_MARKER_DIM, 1.0);
            light = max(light, uUniverse.extended.w * dim);
        }
        light *= (1.0 - resolved) * shown;
        vec4 clip = uUniverse.viewProjection * vec4(relative, 1.0);
        if (light * uUniverse.markers.w < MIN_LIGHT || clip.w <= 0.0)
            return;
        vec2 point = PointShape(light);
        vUv = aCorner * POINT_EXTENT;
        vRadiance = colour * point.x;
        gl_Position = ClipAt(relative, aCorner * point.y);
        return;
    }
    if (resolved <= 0.0)
        return;
    float pixelArea = pixel * pixel;

    if (pass == PASS_GALAXY_DISCS)
    {
        if (shape != SHAPE_SPIRAL && shape != SHAPE_BARRED_SPIRAL && shape != SHAPE_IRREGULAR)
            return;
        // A disc in 3D: brighter seen edge-on (its light through a longer path), as it is thin
        vec3 major = aMajor.xyz;
        vec3 minorAxis = normalize(cross(aNormal.xyz, major));
        float facing = max(abs(dot(aNormal.xyz, viewDirection)), 0.12);
        float scale = DISC_SCALE * radius;
        vRadiance = aColour.rgb * surfaceGain * luminosity * (1.0 - bulge) / (2.0 * PI * scale * scale) / REFERENCE_FLUX * pixelArea * resolved / facing;
        vUv = aCorner * DISC_EXTENT;
        vec3 corner = relative + (major * aCorner.x + minorAxis * aCorner.y) * radius * DISC_EXTENT;
        gl_Position = uUniverse.viewProjection * vec4(corner, 1.0);
        gl_Position.z = 0.5 * gl_Position.w;
        return;
    }

    // Facing the camera: spheroids, bulges and nebulae, drawn out along their major axes as seen
    vec3 major = aMajor.xyz - dot(aMajor.xyz, viewDirection) * viewDirection;
    if (dot(major, major) < 1e-8)
        major = vec3(uUniverse.view[0][0], uUniverse.view[1][0], uUniverse.view[2][0]);
    major = normalize(major);
    vec3 minorAxis = cross(viewDirection, major);
    float ratio = aMajor.w;
    float extent;
    if (pass == PASS_GALAXY_SPHEROIDS)
    {
        bool elliptical = shape == SHAPE_ELLIPTICAL;
        if (!elliptical && bulge <= 0.0)
            return;
        // Giant ellipticals are concentrated; dwarf spheroidals diffuse
        float concentration = smoothstep(8.0, 10.5, log(max(luminosity, 1.0)) / log(10.0));
        float core = (elliptical ? mix(0.45, ELLIPTICAL_CORE * 0.7, concentration) : BULGE_CORE * (0.6 + 2.0 * bulge)) * radius;
        ratio = elliptical ? ratio : 0.85;
        // A cored profile, (1 + r^2)^-2 in core radii, holding all of its light
        vRadiance = aColour.rgb * surfaceGain * luminosity * (elliptical ? 1.0 : bulge) / (PI * core * core * ratio) / REFERENCE_FLUX * pixelArea * resolved;
        extent = SPHEROID_EXTENT * core;
        vUv = aCorner * SPHEROID_EXTENT;
    }
    else if (pass == PASS_NEBULA_SPRITES)
    {
        // Its light spread over its disc (the profile averages about 1 inside its radius)
        // (Coloured by its pattern)
        vRadiance = vec3(surfaceGain * luminosity / (PI * radius * radius * ratio) / REFERENCE_FLUX * pixelArea * resolved);
        extent = NEBULA_EXTENT * radius;
        vUv = aCorner * NEBULA_EXTENT;
    }
    else
    {
        return;
    }
    vec3 corner = relative + (major * aCorner.x + minorAxis * aCorner.y * ratio) * extent;
    gl_Position = uUniverse.viewProjection * vec4(corner, 1.0);
    gl_Position.z = 0.5 * gl_Position.w;
}
