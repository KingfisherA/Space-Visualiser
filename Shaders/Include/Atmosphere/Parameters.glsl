#ifndef SV_ATMOSPHERE_PARAMETERS_GLSL
#define SV_ATMOSPHERE_PARAMETERS_GLSL

// The atmosphere being rendered or precomputed, read from a uniform buffer so one compiled shader
// serves every planet. Mirrors SV::Atmosphere::AtmosphereUniforms (std140).
//
// Define ATMOSPHERE_SET / ATMOSPHERE_BINDING before including to choose where the block lives.

#include "Atmosphere/Definitions.glsl"

#ifndef ATMOSPHERE_SET
#define ATMOSPHERE_SET 0
#endif
#ifndef ATMOSPHERE_BINDING
#define ATMOSPHERE_BINDING 0
#endif

layout(std140, set = ATMOSPHERE_SET, binding = ATMOSPHERE_BINDING) uniform AtmosphereUniforms
{
    vec4 solarIrradiance;      // xyz, w = sun angular radius
    vec4 rayleighScattering;   // xyz, w = bottom radius
    vec4 mieScattering;        // xyz, w = top radius
    vec4 mieExtinction;        // xyz, w = Mie phase function g
    vec4 absorptionExtinction; // xyz, w = mu_s min
    vec4 groundAlbedo;         // xyz
    // (width, exp_term, exp_scale, linear_term) of each density profile layer:
    // [0, 1] Rayleigh, [2, 3] Mie, [4, 5] absorption
    vec4 densityLayers[6];
    // constant_term of each layer above: [0].xyzw = layers 0-3, [1].xy = layers 4-5
    vec4 densityConstants[2];
} uAtmosphere;

DensityProfileLayer GetDensityLayer(int index, float constantTerm)
{
    vec4 layer = uAtmosphere.densityLayers[index];
    return DensityProfileLayer(layer.x, layer.y, layer.z, layer.w, constantTerm);
}

AtmosphereParameters GetAtmosphere()
{
    vec4 c0 = uAtmosphere.densityConstants[0];
    vec4 c1 = uAtmosphere.densityConstants[1];

    DensityProfile rayleighDensity = DensityProfile(
        DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](GetDensityLayer(0, c0.x), GetDensityLayer(1, c0.y)));
    DensityProfile mieDensity = DensityProfile(
        DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](GetDensityLayer(2, c0.z), GetDensityLayer(3, c0.w)));
    DensityProfile absorptionDensity = DensityProfile(
        DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](GetDensityLayer(4, c1.x), GetDensityLayer(5, c1.y)));

    return AtmosphereParameters(
        uAtmosphere.solarIrradiance.xyz,
        uAtmosphere.solarIrradiance.w,
        uAtmosphere.rayleighScattering.w,
        uAtmosphere.mieScattering.w,
        rayleighDensity,
        uAtmosphere.rayleighScattering.xyz,
        mieDensity,
        uAtmosphere.mieScattering.xyz,
        uAtmosphere.mieExtinction.xyz,
        uAtmosphere.mieExtinction.w,
        absorptionDensity,
        uAtmosphere.absorptionExtinction.xyz,
        uAtmosphere.groundAlbedo.xyz,
        uAtmosphere.absorptionExtinction.w);
}

#define ATMOSPHERE GetAtmosphere()

#include "Atmosphere/Functions.glsl"

#endif // SV_ATMOSPHERE_PARAMETERS_GLSL
