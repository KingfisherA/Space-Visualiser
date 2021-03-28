#version 460

#include <Shaders\Universe\Planet\Ring Headers\Planet Rings.f.h.glsl>

in vec3 lightVector;
in vec3 normalVector;
in vec3 toCameraVector;
in vec2 texCoords;
//
out vec4 fragColour;
//
uniform sampler2D surfaceMap;
uniform sampler2D specularMap;
uniform sampler2D cloudsMap;
uniform sampler2D nightMap;
//
uniform vec3 lightColour;
uniform float shineDamper;
uniform float reflectivity;
uniform float time = 0.0f;
//
uniform vec3 nightMapTint = vec3(1.0, 1.0, 0.769) * 0.65f;
const float ambientLight = 0.15f;
//
const float shadowOffset = 0.015f;
const float shadowDarkness = 0.1f;
const float ringDarkness = 0.0f;
//

void main(void)
{
	  vec3 unitNormal = normalize(normalVector);
	  vec3 unitLightVector = normalize(lightVector);

	  float dotResult = dot(unitNormal, unitLightVector);
	  float brightness = max(dotResult, ambientLight);
	  vec3 diffuseColour = brightness * lightColour;
	  vec3 invDiffuseColour = (1.0f - brightness) * lightColour;

	  vec3 surfaceMapColour = texture(surfaceMap, texCoords).rgb * diffuseColour;
	  surfaceMapColour *= 0.65f;

	  vec3 nightMapColour = texture(nightMap, texCoords).rgb * nightMapTint;

	  surfaceMapColour = mix(nightMapColour, surfaceMapColour, (dotResult + 1.0f) *0.5f);

     float specReflectivity = texture(specularMap, texCoords).r * reflectivity;

	  //Phong reflections
	  if(specReflectivity > 0.0f)
	  {
		  vec3 unitToCameraVector = normalize(toCameraVector);
		  vec3 lightDirection = -unitLightVector;
		  vec3 reflectedLightVector = reflect(lightDirection, unitNormal);

		  float specularFactor = max(dot(reflectedLightVector, unitToCameraVector), 0.0f);
		  float dampedFactor = pow(specularFactor, shineDamper);
		  vec3 finalSpecular = dampedFactor * specReflectivity * lightColour;

		  surfaceMapColour+= finalSpecular;
	  }

	  vec2 cloudTexCoords = texCoords + vec2(time, 0.0f);

	  vec4 cloudsMapColour = texture(cloudsMap, cloudTexCoords);

	  float cloudShadowOffset = -((1.0f - dotResult) * shadowOffset);
	  vec4 cloudsShadowColour = texture(cloudsMap, cloudTexCoords + vec2(cloudShadowOffset));

	  //Cloud shadows
	  surfaceMapColour.rgb *= max(1.0f - cloudsShadowColour.r, shadowDarkness);

      //Clouds
	  surfaceMapColour.rgb *= 1.0f - cloudsMapColour.r;
	  surfaceMapColour.rgb += cloudsMapColour.rgb * cloudsMapColour.r * diffuseColour;

	  const float spread = 0.5f;
float theta = (1.0f - (dot(unitNormal, normalize(toCameraVector)) + 1.0f) * 0.5f) * spread;
float theta2 = (dotResult + 1.0f);
     
	surfaceMapColour += mix(vec3(0.0f), vec3(76, 113, 150) / 255.0f, clamp(theta * theta2, 0.0f, 1.0f)) * 4.0f;

	float ringShadow = RingShadowFragment(ringDarkness);

	float mixAmount  = clamp((1.0f - ((dotResult + 1.0f) * 0.5f) )*2.0f, 0.0f, 1.0f);
surfaceMapColour = surfaceMapColour * min(ringShadow + 1.0f * mixAmount, 1.0f);

	fragColour = vec4(surfaceMapColour, 1.0f);
}

//Blinn phong reflections
//	  if(reflectivity > 0.0f)
//	  {
//		  vec3 unitToCameraVector = normalize(toCameraVector);
//		  vec3 lightDirection = -unitLightVector;
//
//		   vec3 halfwayDir = normalize(unitLightVector + unitToCameraVector);  
//		  float specularFactor = max(dot(unitNormal, halfwayDir), 0.0f);
//		  float dampedFactor = pow(specularFactor, shineDamper);
//		  vec3 finalSpecular = dampedFactor * reflectivity * lightColour;
//
//		  fragColour.rgb += finalSpecular;
//	  }