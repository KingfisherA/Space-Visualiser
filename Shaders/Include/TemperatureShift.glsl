#ifndef SV_TEMPERATURE_SHIFT_GLSL
#define SV_TEMPERATURE_SHIFT_GLSL

//Function for calculating star colour temperature shift

vec3 ColourTemperatureShift(float temperature) 
{
	return vec3(temperature * (0.0534 / 255.0) - (43.0 / 255.0), 
						  temperature * (0.0628 / 255.0) - (77.0 / 255.0), 
						  temperature * (0.0735 / 255.0) - (115.0 / 255.0));
}

#endif // SV_TEMPERATURE_SHIFT_GLSL
