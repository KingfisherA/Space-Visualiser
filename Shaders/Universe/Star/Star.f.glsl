#version 460

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//Include necessary files using non-standard include directive
#include "Shaders/Maths/Noise/noise4D.glsl"
#include "Shaders/Maths/Noise/NoiseFunction.h.glsl"
#include "Shaders/Universe/Star/TemperatureShift.h.glsl"
//
in vec3 v_Vertex;
in vec3 v_CamVector;
in vec3 v_NormalVector;
in vec3 v_StarPropeties; //Radius, Star Temperature, Noise Octaves
in vec3 v_StarColour;
//
uniform float u_time;
//
out vec4 fragColour;
//

void main()
{
	//Star colour
	vec3 colour = v_StarColour; /*vec3(0.250980392, 0.878431373, 0.81568627451);*/

	float radius = v_StarPropeties.x;
	float starTemp = v_StarPropeties.y;
	int noiseOctaves = int(ceil(v_StarPropeties.z));

     if (noiseOctaves == 0)
	 {
		 colour += ColourTemperatureShift(starTemp);
	 }
	 else
	{
		//Granule noise
		vec4 position = vec4(v_Vertex, u_time);
		float granuleNoise = (noise(position, noiseOctaves, 40.0, 0.7) + 1.0) * 0.5;
	
		//Sunspot noise
		float sunSpotIntensity = 0.4f;
		float sunSpotFrequency = 0.75f;
		//
		vec4 sPosition = position * radius;
		float t1 = snoise(sPosition * sunSpotFrequency) - sunSpotIntensity;
		float t2 = snoise((sPosition + radius) * sunSpotFrequency) - sunSpotIntensity;
		float sunSpotNoise = (max(t1, 0.0) * max(t2, 0.0)) * 2.0;

		//Total noise added to colour
		colour += ColourTemperatureShift(starTemp);
		colour *= 0.2f + (granuleNoise - sunSpotNoise);
	}

	colour *= 0.75f;

	//Depth Lighting
	//Angle between camera and this vertex for lighting depth
	float theta = (dot(normalize(v_NormalVector), normalize(v_CamVector)) + 1.0f) * 0.5f;
	colour *= theta;

	//Output fragment colour
	fragColour = vec4(colour, 1.0f);
}