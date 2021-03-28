#version 330
  
#include ".\Shaders\Atmosphere\Header Files\ScatteringDefintions.h.glsl"
#pragma insert(atmoParameters)
#include ".\Shaders\Atmosphere\Header Files\ScatteringFunctions.h.glsl"
   
   layout(location = 0) out vec3 delta_irradiance;
    layout(location = 1) out vec3 irradiance;
    uniform sampler2D transmittance_texture;
    void main() 
	{
      delta_irradiance = ComputeDirectIrradianceTexture(
          ATMOSPHERE, transmittance_texture, gl_FragCoord.xy);
      irradiance = vec3(0.0);
    }