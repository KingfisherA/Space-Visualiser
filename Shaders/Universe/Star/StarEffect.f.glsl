#version 150

//Include necessary files using non-standard include directive
#include "Shaders/Noise/noise3D.glsl"
//
in vec2 textureCoordinate;
in vec3 fVertex;
//
out vec4 fragColour;
//
uniform sampler2D glowTexture;
uniform sampler2D spectrumTexture;
uniform float angle;
uniform float blend = 0.0f;
//
uniform float textureCoordinateU;
//

void main(void)
{
	vec3 glowColour = texture(glowTexture, textureCoordinate).rgb;
	//Add spectrum colour
	glowColour.rgb *= texture(spectrumTexture, vec2(textureCoordinateU, 1.0 - glowColour.r + 0.125)).rgb;

	if (blend > 0.225f)
	{
		vec2 distanceVector = normalize(fVertex).xy;
		float spikeVal = snoise(vec3(distanceVector, angle) * 15.5) + 0.2;
 
		float distanceVec = length(fVertex);
		float spikeBrightness = ((1.0 / pow(distanceVec + 0.15, 0.5)) - 1.0);
		spikeBrightness = spikeBrightness * 0.02 * clamp(spikeVal, 0.0, 1.0);
 
		glowColour.rgb += spikeBrightness;
	}

	 fragColour = vec4(glowColour * 2.0f, blend);
}