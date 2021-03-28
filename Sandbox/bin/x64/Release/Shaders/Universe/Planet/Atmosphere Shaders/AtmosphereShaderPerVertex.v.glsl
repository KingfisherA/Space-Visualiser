#version 330 compatibility

//Vertex position attribute
in vec3 vertex;
//
//Matricies for transformation
uniform mat4 transformationMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

// The camera's current position
uniform vec3 v3CameraPos;
// The direction std::vector to the light source
uniform vec3 v3LightPos;
// 1 / pow(wavelength, 4) for the red, green, and blue channels
uniform vec3 v3InvWavelength;
//uniform float fCameraHeight;		// The camera's current height
//uniform float fCameraHeight2;		// fCameraHeight^2
// Kr * ESun
uniform float fKrESun;
// Km * ESun
uniform float fKmESun;
// Kr * 4 * PI
uniform float fKr4PI;
// Km * 4 * PI
uniform float fKm4PI;
// The outer (atmosphere) radius
uniform float fOuterRadius;
// fOuterRadius^2
uniform float fOuterRadius2;
// The inner (planetary) radius
uniform float fInnerRadius;
// fInnerRadius^2
uniform float fInnerRadius2;
// 1 / (fOuterRadius - fInnerRadius)
uniform float fScale;
// The scale depth (i.e. the altitude at which the atmosphere's average density is found)
uniform float fScaleDepth;
// fScale / fScaleDepth
uniform float fScaleOverScaleDepth;
//The current planet's translation std::vector
uniform vec3 translation;

//The number of samples that should be taken for each ray cast
uniform int Samples;

out vec3 v3Direction;
out vec3 v3LightPosPass;
out vec3 rayleighColour;
out vec3 mieColour;

//Scale an angle and apply exponentiation
float scale(float fCos)
{
    float x = 1.0 - fCos;
    return fScaleDepth * exp(-0.00287 + x * (0.459 + x * (3.83 + x * (-6.80 + x * 5.25))));
}

void main(void)
{
    vec3 v3CameraPosTransformed = v3CameraPos - translation;
   //Calculate the camera height (the length of the std::vector) using the dot product but leave it squared to prevent
  //unnessesary square rooting
  float fCameraHeight2 = dot(v3CameraPosTransformed, v3CameraPosTransformed);
    //Now perform the square root of fCameraHeight2 to get the actual length of the std::vector
  float fCameraHeight = sqrt(fCameraHeight2);

    // Get the ray from the camera to the vertex and its length (which is the far point of the ray passing through the atmosphere)
	vec4 worldPosition = transformationMatrix * vec4(vertex, 1.0f);
    vec3 v3Pos = worldPosition.xyz - translation;
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
	rayleighColour = v3FrontColor * (v3InvWavelength * fKrESun);
    mieColour = v3FrontColor * fKmESun;

    //Pass the direction from the vertex to the camera to the fragment shader
	v3Direction = v3CameraPosTransformed - v3Pos;

    //Pass the light position to the fragment shader
	v3LightPosPass = v3LightPos;

	gl_Position = projectionMatrix * viewMatrix * worldPosition;
}