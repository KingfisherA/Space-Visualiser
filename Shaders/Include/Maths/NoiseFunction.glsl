#ifndef SV_NOISE_FUNCTION_GLSL
#define SV_NOISE_FUNCTION_GLSL

#include "Maths/Noise4D.glsl"

//Noise Function for customizability with GPU noise
//Created for use with the noise library

 float noise(vec4 position, int octaves, float frequency, float persistence) 
 {
    float total = 0.0; // Total value so far
    float maxAmplitude = 0.0; // Accumulates highest theoretical amplitude
    float amplitude = 1.0;
    for (int i = 0; i < octaves; i++) 
	{
        // Get the noise sample
        total += snoise(position * frequency) * amplitude;
        // Make the wavelength twice as small
        frequency *= 2.0;
        // Add to our maximum possible amplitude
        maxAmplitude += amplitude;
        // Reduce amplitude according to persistence for the next octave
        amplitude *= persistence;
    }
    // Scale the result by the maximum amplitude
    return total / maxAmplitude;
}

#endif // SV_NOISE_FUNCTION_GLSL
