#version 440

// #extension GL_ARB_gpu_shader_fp64 : enable
// #pragma optionNV(fastmath off)
// #pragma optionNV(fastprecision off)

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

#pragma insert(texProperties)
#include ".\Shaders\Atmosphere\Header Files\ScatteringFunctions.h.glsl"

#pragma insert(radianceAPI)
#pragma insert(useLuminance)
#include ".\Shaders\Atmosphere\Header Files\AtmosphereHeader.h.glsl"

//World Variables
uniform vec3 uCameraPos;
uniform mat4 uTransformMatrix;

//Planet Properties
uniform vec3 uPlanetCentre;

//Lighting Settings
uniform float uAtmoExposure;
uniform vec3 uWhitePoint;
uniform vec3 uSunPosition;
uniform vec2 uSunSize;

//Atmosphere Decay Properties
uniform float uAlphaRad = 3.0f; //Outer width of alpha gradient
uniform float uTime;

//From vertex shader
in vec3 vVertex;
in vec3 vNormal;

layout(location = 0) out vec4 pixColour;

void main()
{
	vec3 scaledCam = uCameraPos * AtmoParams.top_radius;
	vec3 scaledVertex = vVertex * AtmoParams.top_radius;

	vec3 viewDirection = normalize(scaledVertex - scaledCam);

	float tca = dot(scaledCam, viewDirection);
	float d2 = dot(scaledCam, scaledCam) - tca * tca;
	float thc = sqrt(AtmoParams.bottom_radius * AtmoParams.bottom_radius - d2);

	if (tca - thc < 0.0 && tca + thc < 0.0)
	{
		discard;
	}

	vec3 unitSunVector = normalize(uSunPosition * AtmoParams.top_radius);
	vec3 unitNormal = normalize(vNormal * AtmoParams.top_radius);
	vec3 unitCameraVector = normalize(scaledCam);

	vec3 transmittance;
	vec3 radiance = GetSkyRadianceNew(scaledCam, viewDirection,
									  scaledVertex, unitSunVector, transmittance);

	pixColour = vec4(pow(vec3(1.) - exp(-radiance / uWhitePoint * uAtmoExposure), vec3(1.0 / 2.2)),
					 1.0f);
}
