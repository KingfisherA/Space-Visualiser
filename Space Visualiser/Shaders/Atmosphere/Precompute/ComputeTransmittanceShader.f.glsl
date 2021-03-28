#version 330

#include ".\Shaders\Atmosphere\Header Files\ScatteringDefintions.h.glsl"
#pragma insert(atmoParameters)
#include ".\Shaders\Atmosphere\Header Files\ScatteringFunctions.h.glsl"

layout(location = 0) out vec3 transmittance;
void main()
{
  transmittance = ComputeTransmittanceToTopAtmosphereBoundaryTexture(ATMOSPHERE, gl_FragCoord.xy);
}