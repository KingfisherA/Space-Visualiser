#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(set = 1, binding = 0) uniform sampler2D transmittance_texture;
layout(set = 1, binding = 1) uniform sampler3D scattering_density_texture;

layout(location = 0) out vec4 delta_multiple_scattering;
layout(location = 1) out vec4 scattering; // Accumulated (additive blending)

void main()
{
    float nu;
    vec3 deltaMultipleScattering = ComputeMultipleScatteringTexture(
        ATMOSPHERE, transmittance_texture, scattering_density_texture,
        vec3(gl_FragCoord.xy, uPass.layer + 0.5), nu);
    delta_multiple_scattering = vec4(deltaMultipleScattering, 1.0);
    scattering = vec4(luminance_from_radiance * deltaMultipleScattering / RayleighPhaseFunction(nu), 0.0);
}
