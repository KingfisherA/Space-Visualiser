#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(set = 1, binding = 0) uniform sampler2D transmittance_texture;

layout(location = 0) out vec4 delta_rayleigh;
layout(location = 1) out vec4 delta_mie;
layout(location = 2) out vec4 scattering;

void main()
{
    vec3 rayleigh, mie;
    ComputeSingleScatteringTexture(ATMOSPHERE, transmittance_texture,
                                   vec3(gl_FragCoord.xy, uPass.layer + 0.5), rayleigh, mie);
    delta_rayleigh = vec4(rayleigh, 1.0);
    delta_mie = vec4(mie, 1.0);
    // Combined scattering texture: single Mie scattering's red channel goes in alpha
    scattering = vec4(luminance_from_radiance * rayleigh, (luminance_from_radiance * mie).r);
}
