#version 410
#extension GL_ARB_shading_language_420pack : require

#include "Atmosphere/Precompute.glsl"

layout(location = 0) out vec4 transmittance;

void main()
{
    transmittance = vec4(ComputeTransmittanceToTopAtmosphereBoundaryTexture(ATMOSPHERE, gl_FragCoord.xy), 1.0);
}
