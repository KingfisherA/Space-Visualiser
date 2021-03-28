#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

#pragma insert(texProperties)
#include ".\Shaders\Atmosphere\Header Files\ScatteringFunctions.h.glsl"

#pragma insert(radianceAPI)
#pragma insert(useLuminance)
#include ".\Shaders\Atmosphere\Header Files\AtmosphereHeader.h.glsl"

//World Properties
uniform vec3 uCameraPos;
uniform float uTime = 0.0f;

//Planet Properties
uniform vec3 uPlanetCentre;
uniform float uExternalAtmoIntensity = 1.0f;

//Lighting Variables
uniform float uAtmoExposure;
uniform vec3 uWhitePoint = vec3(1.0f);
uniform vec3 uSunPosition;
uniform vec2 uSunSize;

//Texture Maps
uniform sampler2D uDiffuseMap;
uniform OptionalTex2D uSpecularMap;
uniform OptionalTex2D uCloudsMap;
uniform OptionalTex2D uRingsShadowMap;
uniform OptionalTex2D uNightMap;

in vec3 vVertex;
in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vToCamVector;
in vec2 vRingShadowParams; //(Ring shadow x-texture coordinate, Ring shadow fade)

layout(location = 0) out vec4 pixColour;
//
const vec3 kGroundAlbedo = vec3(0.0, 0.0, 0.04);

//
void SpecularPhong(float specReflectivity, vec3 unitNormal, vec3 unitToCameraVector, vec3 unitSunVector, vec3 lightColour,
				   float shineDamper, inout vec3 surfaceMapColour)
{
	// Phong reflectionsa
	if (specReflectivity > 0.0f)
	{
		vec3 lightDirection = -unitSunVector;
		vec3 reflectedLightVector = reflect(lightDirection, unitNormal);

		float specularFactor = max(dot(reflectedLightVector, unitToCameraVector), 0.0f);
		float dampedFactor = pow(specularFactor, shineDamper);
		vec3 finalSpecular = dampedFactor * specReflectivity * lightColour;

		surfaceMapColour += finalSpecular;
	}
}

//
float ApproximateLuminance(in vec3 colour)
{
	return (0.2126f * colour.r + 0.7152f * colour.g + 0.0722f * colour.b);
}

void main()
{
	vec3 unitSunVector = normalize(uSunPosition);
	vec3 unitNormal = normalize(vNormal);
	vec3 scaledVertex = vVertex * AtmoParams.bottom_radius; //Must not have any translation
	vec3 scaledCamera = (uCameraPos - uPlanetCentre) * AtmoParams.bottom_radius;
	vec3 viewDirection = normalize(scaledVertex - scaledCamera);

	vec3 scaledCamTop = (uCameraPos - uPlanetCentre) * AtmoParams.top_radius;
	vec3 scaledVertexTop = vVertex * AtmoParams.top_radius;

	float tca = dot(scaledCamTop, viewDirection);
	float thc = sqrt(AtmoParams.bottom_radius * AtmoParams.bottom_radius -
					 (dot(scaledCamTop, scaledCamTop) - tca * tca));

	vec3 transmittance;
	vec3 radiance = GetSkyRadianceNew(scaledCamTop, viewDirection,
									  scaledVertexTop, unitSunVector, transmittance);

	radiance = pow(vec3(1.) - exp(-radiance / uWhitePoint * uAtmoExposure), vec3(1.0 / 2.2));

	float alphaValue = exp(ApproximateLuminance(radiance)) - 1.0f;
	pixColour.a = alphaValue;
	vec3 nightColour = vec3(0.0f);

	//Test for sphere intersection
	if (tca - thc < 0.0 && tca + thc < 0.0)
	{
		const float rotationSpeed = 0.3f;
		vec2 offsetCoords = vTexCoord + vec2(uTime, 0.0f) * rotationSpeed;
		vec3 surfaceMapColour = texture(uDiffuseMap, offsetCoords).rgb * 0.5f;

		if (uSpecularMap.hasTexture)
		{
			const float damper = 80.0f;
			float specReflectivity = texture(uSpecularMap.texSampler, offsetCoords).r * 8.0f;

			SpecularPhong(specReflectivity, unitNormal, normalize(vToCamVector), unitSunVector, vec3(0.1f),
						  damper, surfaceMapColour);
		}

		float dotResult = dot(unitNormal, unitSunVector);
		vec3 diffuseColour = dotResult * vec3(1.3f);

		if (uCloudsMap.hasTexture)
		{
			const float cloudSpeed = 0.5f;
			vec2 cloudTexCoords = vTexCoord + vec2(uTime, 0.0f) * cloudSpeed;

			const float shadowOffset = 0.015f;
			const float shadowDarkness = 0.02f;
			vec4 cloudsMapColour = texture(uCloudsMap.texSampler, cloudTexCoords);
			float cloudShadowOffset = -((1.0f - dotResult) * shadowOffset);

			vec4 cloudsShadowColour = texture(uCloudsMap.texSampler, cloudTexCoords + vec2(cloudShadowOffset));
			// Cloud shadows
			surfaceMapColour.rgb *= max(1.0f - cloudsShadowColour.r, shadowDarkness);
			// Clouds
			surfaceMapColour.rgb *= 1.0f - cloudsMapColour.r;
			surfaceMapColour.rgb += cloudsMapColour.rgb * cloudsMapColour.r * diffuseColour;
		}

		float luminance = clamp(abs(ApproximateLuminance(radiance)), 0.0f, 1.0f);

		surfaceMapColour.rgb *= luminance;

		radiance = radiance * uExternalAtmoIntensity + surfaceMapColour;

		if (uRingsShadowMap.hasTexture)
		{
			// Calculate shadow by rings
			float ringsShadow = 1.0f;
			ringsShadow *= 1.0 - vRingShadowParams.y * texture(uRingsShadowMap.texSampler, vec2(vRingShadowParams.x, 0.75)).a;
			radiance *= ringsShadow;
		}

		if (uNightMap.hasTexture)
		{
			const float spreadFactor = 6.0f;
			const float offsetFactor = 0.13f;
			float invLuminance = smoothstep(0.0f, 1.0f, max(1.0f - (dotResult + offsetFactor) * spreadFactor, 0.0f));
			nightColour = texture(uNightMap.texSampler, offsetCoords).rgb * invLuminance;
		}

		alphaValue = 0.0f;
		pixColour.a = 1.0f;
	}

	radiance.rgb += nightColour;

	pixColour.rgb = radiance;
}