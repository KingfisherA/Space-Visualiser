#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(set = 1, binding = 0) uniform sampler2D transmittance_texture;

layout(location = 0) out vec4 delta_irradiance;
layout(location = 1) out vec4 irradiance;

void main()
{
    delta_irradiance = vec4(ComputeDirectIrradianceTexture(ATMOSPHERE, transmittance_texture, gl_FragCoord.xy), 1.0);
    // The direct irradiance is not stored in the final irradiance texture, only the sky's
    irradiance = vec4(0.0);
}
