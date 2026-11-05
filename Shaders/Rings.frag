#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Frame.glsl"
#include "RingsPushConstants.glsl"

// Set 1: the ring map
layout(set = 1, binding = 0) uniform sampler2D uRingMap;

// Set 2: the planet's atmosphere, for the planet's shadow (sunlight grazing the planet is filtered
// by its atmosphere)
#define ATMOSPHERE_SET 2
#define ATMOSPHERE_BINDING 0
#include "Atmosphere/Parameters.glsl"
layout(set = 2, binding = 1) uniform sampler2D transmittanceTexture;

#define RING_MAP uRingMap
#define TRANSMITTANCE_TEXTURE transmittanceTexture
#include "Rings.glsl"

layout(location = 0) in vec3 vWorldPosition;

// Premultiplied: rgb is the light the rings send to the camera, a is how much of what is behind
// them they hide
layout(location = 0) out vec4 pixColour;


void main()
{
    float innerRadius = uRings.rings.x;
    float outerRadius = uRings.rings.y;
    float unitsPerRadius = uRings.axes[0].w;
    float polarRatio = uRings.axes[1].w;

    // Planet space: centred on the planet, y along its axis (the ring plane is y = 0), in
    // equatorial radii
    mat3 toPlanet = transpose(GetRingsPlanetRotation());
    vec3 centre = uRings.transform[3].xyz;
    vec3 position = toPlanet * (vWorldPosition - centre) / unitsPerRadius;
    vec3 camera = toPlanet * (uFrame.cameraPosition.xyz - centre) / unitsPerRadius;
    vec3 toViewer = normalize(camera - position);
    vec3 sunOffset = uFrame.sun.xyz - centre;
    vec3 toSun = normalize(toPlanet * sunOffset);
    float sunAngularRadius = asin(min(uFrame.sun.w / length(sunOffset), 1.0));

    // Filter the ring map by the pixel's radial footprint (before any branch, for the derivatives)
    float radius = length(position.xz);
    float u = (radius - innerRadius) / (outerRadius - innerRadius);
    float lod = log2(max(fwidth(u) * float(textureSize(uRingMap, 0).x), 1.0));
    RingSample ring = SampleRings(radius, innerRadius, outerRadius, lod);
    if (ring.opticalDepth <= 1e-4)
        discard;

    // Sunlight scattered towards the camera (reflected from the lit face, or transmitted through
    // to the unlit face) by the ring particles and dust
    vec3 reflectance = RingReflectance(ring, toSun, toViewer);
    vec3 sunlight = PlanetShadow(position, toSun, polarRatio, sunAngularRadius);
    vec3 radiance = reflectance * sunlight * ATMOSPHERE.solar_irradiance / PI;

    float opacity = 1.0 - RingTransmittance(ring, abs(toViewer.y));
    pixColour = vec4(ToneMap(radiance), opacity);
}
