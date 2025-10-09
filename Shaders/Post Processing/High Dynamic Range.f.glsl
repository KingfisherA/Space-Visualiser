#version 150

in vec2 textureCoordinate;
//
out vec4 fragColour;
//
uniform sampler2D texture1;
//
uniform float exposure;
uniform float gamma;
//

void main(void)
{
	//
	fragColour.rgb = texture(texture1, textureCoordinate).rgb;
	//HDR Exposure
	fragColour.rgb = 1.0f - exp(fragColour.rgb * -exposure);
	 //Gamma correction
	 fragColour.rgb = pow(fragColour.rgb, vec3(gamma));
	 //
	 fragColour.a = 1.0f;
	 //
}