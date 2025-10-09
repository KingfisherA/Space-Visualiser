#version 330 compatibility

//The phase function scattering constant
uniform float g;
//g^2
uniform float g2;
//
in vec3 v3Direction;
in vec3 v3LightPosPass;
in vec3 rayleighColour;
in vec3 mieColour;
//
out vec4 fragColour;

void main (void)
{
    //Rayleigh phase scattering
	float fCos = dot(v3LightPosPass, v3Direction) / length(v3Direction);
    float fRayleighPhase = 1.0 + fCos * fCos;

    //Mie phase scattering
	float fMiePhase = (1.0 - g2) / (2.0 + g2) * (1.0 + fCos * fCos) / pow(1.0 + g2 - 2.0 * g * fCos, 1.5);
    vec3 finalColour = 1.0 - exp(-1.5 * (fRayleighPhase * rayleighColour + fMiePhase * mieColour));

	//Output the final calculated scatter colour
    fragColour = vec4(finalColour, 1.0f);
}