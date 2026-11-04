#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(set = 1, binding = 0) uniform sampler3D single_rayleigh_scattering_texture;
layout(set = 1, binding = 1) uniform sampler3D single_mie_scattering_texture;
layout(set = 1, binding = 2) uniform sampler3D multiple_scattering_texture;

layout(location = 0) out vec4 delta_irradiance;
layout(location = 1) out vec4 irradiance; // Accumulated (additive blending)

void main()
{
    vec3 deltaIrradiance = ComputeIndirectIrradianceTexture(
        ATMOSPHERE, single_rayleigh_scattering_texture,
        single_mie_scattering_texture, multiple_scattering_texture,
        gl_FragCoord.xy, uPass.scatteringOrder);
    delta_irradiance = vec4(deltaIrradiance, 1.0);
    irradiance = vec4(luminance_from_radiance * deltaIrradiance, 0.0);
}
