#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Universe.glsl"

layout(location = 0) in vec2 aCorner;  // The quad's corner, in [-1, 1]
layout(location = 1) in vec4 aHigh;    // xyz = position (high part), w = luminosity (stars, in the Sun's)
layout(location = 2) in vec4 aLow;     // xyz = position (low part), w = kind (KIND_*)
layout(location = 3) in vec4 aColour;  // rgb = colour (luminance 1), w = spin period (s, 0 if unknown)

layout(location = 0) out vec2 vCorner;
layout(location = 1) flat out vec3 vRadiance;
layout(location = 2) flat out int vKind;

// Mirrors PointKind in src/SV/Universe/UniverseRenderer.cpp
const int KIND_STAR = 0;
const int KIND_PULSAR = 1;
const int KIND_MILLISECOND_PULSAR = 2;
const int KIND_MAGNETAR = 3;
const int KIND_NEUTRON_STAR = 4;
const int KIND_XRAY_BINARY = 5;
const int KIND_STELLAR_BLACK_HOLE = 6;
const int KIND_SUPERMASSIVE_BLACK_HOLE = 7;
const int KIND_SUN = 8;

// Stars nearer than this are drawn as if this far away (pc), so a star the camera is close to
// doesn't blow up
const float MIN_STAR_DISTANCE = 0.002;
// Points fainter than this (in reference fluxes, after the exposure) aren't drawn
const float MIN_LIGHT = 2e-4;
// Markers dim with their distance beyond the zoom (as the square), to at least this
const float MIN_MARKER_DIM = 0.04;
// Pulsars pulse at their spin rate, slowed to at most this rate (Hz) so the pulses can be seen
const float MAX_PULSE_RATE = 1.5;

float Hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    vKind = int(aLow.w + 0.5);
    vCorner = aCorner;
    vec3 relative = FromCamera(aHigh.xyz, aLow.xyz);
    float distance = length(relative);
    float zoom = uUniverse.stars.z;

    float light;
    if (vKind == KIND_STAR || vKind == KIND_SUN)
    {
        float d = max(distance, MIN_STAR_DISTANCE);
        light = aHigh.w / (d * d) / REFERENCE_FLUX * uUniverse.stars.x * uUniverse.stars.y;
        light *= vKind == KIND_SUN ? uUniverse.stars.w : uUniverse.markers.z;
    }
    else
    {
        // A marker: as bright at any distance within the zoom, dimming beyond it
        float dim = clamp(zoom * zoom / max(distance * distance, 1e-12), MIN_MARKER_DIM, 1.0);
        bool blackHole = vKind == KIND_STELLAR_BLACK_HOLE || vKind == KIND_SUPERMASSIVE_BLACK_HOLE;
        light = (blackHole ? uUniverse.markers.y : uUniverse.markers.x) * dim;
        if (vKind == KIND_SUPERMASSIVE_BLACK_HOLE)
            light *= 12.0;
        // Pulsing (a sharp peak each turn)
        if (vKind == KIND_PULSAR || vKind == KIND_MILLISECOND_PULSAR || vKind == KIND_MAGNETAR)
        {
            float seed = Hash(aHigh.xyz);
            float period = aColour.w > 1.0 / MAX_PULSE_RATE ? aColour.w : 1.0 / MAX_PULSE_RATE * (1.0 + seed);
            float phase = fract(uUniverse.viewport.w / period + seed);
            light *= 0.45 + 2.0 * pow(0.5 + 0.5 * cos(6.28318531 * phase), 12.0);
        }
    }

    light *= Shown(aHigh.xyz, uUniverse.hiddenPoint);
    for (int i = 0; i < MAX_STAR_CLOSE_UPS; i++)
        light *= Shown(aHigh.xyz, uUniverse.hiddenStars[i]);
    vec4 clip = uUniverse.viewProjection * vec4(relative, 1.0);
    if (light * uUniverse.markers.w < MIN_LIGHT || clip.w <= 0.0)
    {
        gl_Position = CULLED;
        vRadiance = vec3(0.0);
        return;
    }
    vec2 shape = PointShape(light);
    // Black holes are drawn as a dark-centred ring a little larger
    if (vKind == KIND_SUPERMASSIVE_BLACK_HOLE || vKind == KIND_STELLAR_BLACK_HOLE)
        shape.y *= vKind == KIND_SUPERMASSIVE_BLACK_HOLE ? 3.0 : 1.6;
    vRadiance = aColour.rgb * shape.x;
    gl_Position = ClipAt(relative, aCorner * shape.y);
}
