#pragma once
#include ".\Shaders\Atmosphere\Header Files\ScatteringFunctions.h.glsl"

#pragma insert(atmoParameters)

//#define AtmoParams ATMOSPHERE
//#define u_SunSpectralRadianceToLuminance SUN_SPECTRAL_RADIANCE_TO_LUMINANCE
//#define u_SkySpectralRadianceToLuminance SKY_SPECTRAL_RADIANCE_TO_LUMINANCE

//vec3 Solar Irradiance, vec3 Rayleigh Scattering, vec3 Mie Scattering, vec3 Mie Extinction,
//vec3 Absorption Extinction, vec3 Ground Albedo
uniform vec3 u_AtmoParams1[6];
//float Sun Angular Radius, float Mu S Min, float bottom radius, float top radius
uniform vec4 u_AtmoParams2;
//float Mie Phase Function G
uniform float u_MiePhaseFunctionG;
//
// uniform DensityProfileLayer u_RayleighDensity[MAX_DENSITY_PROFILE_LAYERS];
// uniform DensityProfileLayer u_MieDensity[MAX_DENSITY_PROFILE_LAYERS];
// uniform DensityProfileLayer u_AbsorptionDensity[MAX_DENSITY_PROFILE_LAYERS];
//vec4, vec4 Rayleigh Density, vec4, vec4 Mie Density, vec4, vec4 Absorption Density
uniform vec4 u_DensityProfiles[MAX_DENSITY_PROFILE_LAYERS * 3];
//float, float Rayleigh Density, float, float Mie Density, float, float Absorption Density
uniform float u_DensityProfileConstant[MAX_DENSITY_PROFILE_LAYERS * 3];
//

#define u_SolarIrradiance u_AtmoParams1[0]
#define u_RayleighScattering u_AtmoParams1[1]
#define u_MieScattering u_AtmoParams1[2]
#define u_MieExtinction u_AtmoParams1[3]
#define u_AbsorptionExtinction u_AtmoParams1[4]
#define u_GroundAlbdeo u_AtmoParams1[5]
#define u_SunAngularRadius u_AtmoParams2.x
#define u_MuSMin u_AtmoParams2.y
#define u_BottomRadius u_AtmoParams2.z
#define u_TopRadius u_AtmoParams2.w

#define u_DensityLayer(layer) DensityProfileLayer(u_DensityProfiles[layer].x, u_DensityProfiles[layer].y, u_DensityProfiles[layer].z, u_DensityProfiles[layer].w, u_DensityProfileConstant[layer])
#define u_RayleighDensity DensityProfile(DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](u_DensityLayer(0), u_DensityLayer(1)))
#define u_MieDensity DensityProfile(DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](u_DensityLayer(2), u_DensityLayer(3)))
#define u_AbsorptionDensity DensityProfile(DensityProfileLayer[MAX_DENSITY_PROFILE_LAYERS](u_DensityLayer(4), u_DensityLayer(5)))

AtmosphereParameters AtmoParams = AtmosphereParameters(u_SolarIrradiance, u_SunAngularRadius, u_BottomRadius, u_TopRadius,
                                                       u_RayleighDensity,
                                                       u_RayleighScattering,
                                                       u_MieDensity,
                                                       u_MieScattering, u_MieExtinction, u_MiePhaseFunctionG,
                                                       u_AbsorptionDensity,
                                                       u_AbsorptionExtinction, u_GroundAlbdeo, u_MuSMin);

uniform vec3 u_SunSpectralRadianceToLuminance;
uniform vec3 u_SkySpectralRadianceToLuminance;

uniform sampler2D transmittanceTexture;
uniform sampler3D scatteringTexture;
uniform sampler3D singleMieScatteringTexture;
uniform sampler2D irradianceTexture;

#ifdef RADIANCE_API_ENABLED
RadianceSpectrum GetSolarRadiance()
{
  return AtmoParams.solar_irradiance /
         (PI * AtmoParams.sun_angular_radius * AtmoParams.sun_angular_radius);
}
RadianceSpectrum GetSkyRadiance(
    Position camera, Direction view_ray, Length shadow_length,
    Direction sun_direction, out DimensionlessSpectrum transmittance)
{
  return GetSkyRadiance(AtmoParams, transmittanceTexture,
                        scatteringTexture, singleMieScatteringTexture,
                        camera, view_ray, shadow_length, sun_direction, transmittance);
}
RadianceSpectrum GetSkyRadianceNew(
    Position camera, Direction view_ray, vec3 vPos,
    Direction sun_direction, out DimensionlessSpectrum transmittance)
{
  return GetSkyRadianceNew(AtmoParams, transmittanceTexture,
                           scatteringTexture, singleMieScatteringTexture,
                           camera, view_ray, vPos, sun_direction, transmittance);
}
RadianceSpectrum GetSkyRadianceToPoint(
    Position camera, Position point, Length shadow_length,
    Direction sun_direction, out DimensionlessSpectrum transmittance)
{
  return GetSkyRadianceToPoint(AtmoParams, transmittanceTexture,
                               scatteringTexture, singleMieScatteringTexture,
                               camera, point, shadow_length, sun_direction, transmittance);
}
IrradianceSpectrum GetSunAndSkyIrradiance(
    Position p, Direction normal, Direction sun_direction,
    out IrradianceSpectrum sky_irradiance)
{
  return GetSunAndSkyIrradiance(AtmoParams, transmittanceTexture,
                                irradianceTexture, p, normal, sun_direction, sky_irradiance);
}
#endif
Luminance3 GetSolarLuminance()
{
  return AtmoParams.solar_irradiance /
         (PI * AtmoParams.sun_angular_radius * AtmoParams.sun_angular_radius) *
         u_SunSpectralRadianceToLuminance;
}
Luminance3 GetSkyLuminance(
    Position camera, Direction view_ray, Length shadow_length,
    Direction sun_direction, out DimensionlessSpectrum transmittance)
{
  return GetSkyRadiance(AtmoParams, transmittanceTexture,
                        scatteringTexture, singleMieScatteringTexture,
                        camera, view_ray, shadow_length, sun_direction, transmittance) *
         u_SkySpectralRadianceToLuminance;
}
Luminance3 GetSkyLuminanceToPoint(
    Position camera, Position point, Length shadow_length,
    Direction sun_direction, out DimensionlessSpectrum transmittance)
{
  return GetSkyRadianceToPoint(AtmoParams, transmittanceTexture,
                               scatteringTexture, singleMieScatteringTexture,
                               camera, point, shadow_length, sun_direction, transmittance) *
         u_SkySpectralRadianceToLuminance;
}
Illuminance3 GetSunAndSkyIlluminance(
    Position p, Direction normal, Direction sun_direction,
    out IrradianceSpectrum sky_irradiance)
{
  IrradianceSpectrum sun_irradiance = GetSunAndSkyIrradiance(
      AtmoParams, transmittanceTexture, irradianceTexture, p, normal,
      sun_direction, sky_irradiance);
  sky_irradiance *= u_SkySpectralRadianceToLuminance;
  return sun_irradiance * u_SunSpectralRadianceToLuminance;
}