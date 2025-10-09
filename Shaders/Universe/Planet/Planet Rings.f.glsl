#version 150

#include<Shaders/Universe/Planet/Ring Headers/Planet Rings.f.h.glsl>

//
in vec3 lightVector;
in vec3 normalVector;
in vec2 texCoord;
//
out vec4 fragColour;
//
uniform vec3 lightColour = vec3(1.0f);
//
uniform sampler2D surfaceMap;
//Constants
const float ambientLight = 0.06f;
//

void main(void)
{
	vec3 unitNormal = normalize(normalVector);
	vec3 unitLightVec = normalize(lightVector);

	float diffuseAmount = max(dot(unitNormal, unitLightVec) , 0.0f);

	vec4 texSamp = texture(surfaceMap, texCoord);
	vec3 diffuseColour = diffuseAmount * texSamp.rgb * lightColour;

	diffuseColour *= RingShadowFragment(ambientLight);

	fragColour = vec4(diffuseColour, texSamp.a);
}