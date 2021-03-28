#version 460

#pragma remove
#include ".\Shaders\GLSL Language Specification.h.glsl"
#pragma endremove

//Include necessary files using non-standard include directive
#include ".\Shaders\Maths\Noise\noise4D.glsl"
#include ".\Shaders\Maths\Noise\NoiseFunction.h.glsl"
#include ".\Shaders\Maths\Utilities.h.glsl"
//Unroll all of the loops to improve performance (Nvidia only)
#pragma optionNV(unroll all)
//
in vec3 v_Vertex;
in vec3 v_starColour;
//
uniform float time;
uniform int noiseOctaves = 5;
//
//0.7f
const float intensity = 3.0f;
const float invintensity = 1.0f / (intensity * 1.3f);
//
out vec4 fragColour;
//

void main()
{
	fragColour = vec4(0.0f);
	if (noiseOctaves > 0)
	{
		float t = time - length(v_Vertex);

		// Offset normal with noise
		const float frequency = 1.5;
		float ox = snoise(vec4(v_Vertex, t) * frequency);
		float oy = snoise(vec4((v_Vertex + 2000.0), t) * frequency);
		float oz = snoise(vec4((v_Vertex + 4000.0), t) * frequency);

		vec3 offsetVec = vec3(ox, oy, oz) * 0.1;
		vec3 distanceFromCenter = normalize(v_Vertex + offsetVec);

		vec3 positionNoise = v_Vertex + noise(vec4(distanceFromCenter, time), noiseOctaves, 2.0, 0.7) * 0.23;

		//float dist = length(positionNoise + offsetVec) * /*3.15*/ 2.7;
		//float brightness = (1.0 / (dist * dist) - 0.1) * 0.7;
		float dist2 = lengthSquared(positionNoise + offsetVec) * 3.0f;
		float brightness = (1.0 / dist2 - 0.1) * intensity;

		fragColour = vec4(v_starColour * brightness, brightness * invintensity);
	}
}