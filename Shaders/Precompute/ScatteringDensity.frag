#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(set = 1, binding = 0) uniform sampler2D transmittance_texture;
layout(set = 1, binding = 1) uniform sampler3D single_rayleigh_scattering_texture;
layout(set = 1, binding = 2) uniform sampler3D single_mie_scattering_texture;
layout(set = 1, binding = 3) uniform sampler3D multiple_scattering_texture;
layout(set = 1, binding = 4) uniform sampler2D irradiance_texture;

layout(location = 0) out vec4 scattering_density;

void main()
{
    scattering_density = vec4(ComputeScatteringDensityTexture(
        ATMOSPHERE, transmittance_texture, single_rayleigh_scattering_texture,
        single_mie_scattering_texture, multiple_scattering_texture,
        irradiance_texture, vec3(gl_FragCoord.xy, uPass.layer + 0.5),
        uPass.scatteringOrder), 1.0);
}
