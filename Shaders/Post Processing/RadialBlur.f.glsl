#version 150

in vec2 textureCoordinates;
//
out vec4 fragColour;
//
uniform sampler2D texture1;
uniform vec2 location;
uniform int samples;

void main (void) 
{
    vec3 RadialBlur = vec3(0.0);
	vec2 RadialBlurCoordinates = textureCoordinates;
    vec2 RadialBlurVector = (location - textureCoordinates) / samples;

    for(int i = 0; i < samples; i++)
	{
        RadialBlur += texture(texture1, RadialBlurCoordinates).rgb;
        RadialBlurCoordinates += RadialBlurVector;
    }

	RadialBlur /= samples;

    fragColour = vec4(RadialBlur, 1.0f);
}