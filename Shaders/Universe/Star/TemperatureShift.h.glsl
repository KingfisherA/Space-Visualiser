//Function for calculating star colour temperature shift
#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

vec3 ColourTemperatureShift(float temperature) 
{
	return vec3(temperature * (0.0534 / 255.0) - (43.0 / 255.0), 
						  temperature * (0.0628 / 255.0) - (77.0 / 255.0), 
						  temperature * (0.0735 / 255.0) - (115.0 / 255.0));
}