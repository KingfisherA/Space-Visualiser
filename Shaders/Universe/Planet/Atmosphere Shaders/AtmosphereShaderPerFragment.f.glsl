#version 330

// The direction std::vector to the light source
uniform vec3 v3LightPos;
// 1 / pow(wavelength, 4) for the red, green, and blue channels
uniform vec3 v3InvWavelength;
// The camera's current height
//uniform float fCameraHeight;		
// fCameraHeight^2
//uniform float fCameraHeight2;		
// The outer (atmosphere) radius
uniform float fOuterRadius;
// fOuterRadius^2
uniform float fOuterRadius2;
// The inner (planetary) radius
uniform float fInnerRadius;
// Kr * ESun
uniform float fKrESun;
// Km * ESun
uniform float fKmESun;
// Kr * 4 * PI
uniform float fKr4PI;
// Km * 4 * PI
uniform float fKm4PI;
// 1 / (fOuterRadius - fInnerRadius)
uniform float fScale;
// The scale depth (i.e. the altitude at which the atmosphere's average density is found)
uniform float fScaleDepth;
// fScale / fScaleDepth
uniform float fScaleOverScaleDepth;
//The phase function scattering constant
uniform float g;
//g^2
uniform float g2;

//The number of samples that should be taken for each ray cast
uniform int Samples;
//
in vec3 v3CameraPosTransformed;
in float fCameraHeight2;
in float fCameraHeight;
in vec3 v3Pos;
//
out vec4 fragColour;

//Scale an angle and apply exponentiation
float scale(float fCos)
{
    float x = 1.0 - fCos;
    return fScaleDepth * exp(-0.00287 + x * (0.459 + x * (3.83 + x * (-6.80 + x * 5.25))));
}

void main(void)
{
    // Get the ray from the camera to the vertex and its length (which is the far point of the ray passing through the atmosphere)
	vec3 v3Ray = v3Pos - v3CameraPosTransformed;
    float fFar = length(v3Ray);
    v3Ray /= fFar;

    // Calculate the closest intersection of the ray with the outer atmosphere (which is the near point of the ray passing through the atmosphere)
	float B = 2.0 * dot(v3CameraPosTransformed, v3Ray);
    float C = fCameraHeight2 - fOuterRadius2;
    float fDet = max(0.0, B * B - 4.0 * C);
    float fNear = 0.5 * (-B - sqrt(fDet));

    // Calculate the ray's starting position, then calculate its scattering offset
	vec3 v3Start = v3CameraPosTransformed + v3Ray * fNear;
    fFar -= fNear;
    float fStartAngle = dot(v3Ray, v3Start) / fOuterRadius;
    float fStartDepth = exp(-1.0 / fScaleDepth);
    float fStartOffset = fStartDepth * scale(fStartAngle);

    // Initialize the scattering loop variables
	float fSampleLength = fFar / Samples;
    float fScaledLength = fSampleLength * fScale;
    vec3 v3SampleRay = v3Ray * fSampleLength;
    vec3 v3SamplePoint = v3Start + v3SampleRay * 0.5;

    // Now loop through the sample rays
	vec3 v3FrontColor = vec3(0.0f);
    for (int i = 0; i < Samples; i++)
	{
        float fHeight = length(v3SamplePoint);
        float fDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fHeight));

        float fLightAngle = dot(v3LightPos, v3SamplePoint) / fHeight;
        float fCameraAngle = dot(v3Ray, v3SamplePoint) / fHeight;

        float fScatter = (fStartOffset + fDepth * (scale(fLightAngle) - scale(fCameraAngle)));
        vec3 v3Attenuate = exp(-fScatter * (v3InvWavelength * fKr4PI + fKm4PI));

        v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
        v3SamplePoint += v3SampleRay;
    }

	// Finally, sca-le the Mie and Rayleigh colors and set up the varying variables for the fragment shader
	vec3 rayleighColour = v3FrontColor * (v3InvWavelength * fKrESun);
    vec3 mieColour = v3FrontColor * fKmESun;

    //The direction from the vertex to the camera to the fragment shader
	vec3 v3Direction = v3CameraPosTransformed - v3Pos;

	//Rayleigh phase scattering
	float fCos = dot(v3LightPos, v3Direction) / length(v3Direction);
    float fRayleighPhase = 1.0 + fCos * fCos;

    //Mie phase scattering
	float fMiePhase = (1.0 - g2) / (2.0 + g2) * (1.0 + fCos * fCos) / pow(1.0 + g2 - 2.0 * g * fCos, 1.5);
    vec3 finalColour = 1.0 - exp(-1.5 * (fRayleighPhase * rayleighColour + fMiePhase * mieColour));

	float alpha = (finalColour.r + finalColour.g + finalColour.b)/3.0f;
	alpha *= 0.85f;

	//Output the final calculated scatter colour
    fragColour = vec4(finalColour, alpha);
}