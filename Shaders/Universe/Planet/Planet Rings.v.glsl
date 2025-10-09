#version 150

#include <Shaders/Universe/Planet/Ring Headers/Planet Rings.v.h.glsl>

//
in vec3 position;
in vec3 normal;
in vec2 textureCoords;
//
out vec3 lightVector;
out vec3 normalVector;
out vec2 texCoord;
//
uniform mat4 transformMatrix;
uniform mat3 normalMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
//
uniform vec3 lightPosition;
//

void main()
{
    vec4 positionWorld = transformMatrix * vec4(position, 1.0f);
    vec4 positionTrans = projectionMatrix * viewMatrix * positionWorld;

	RingShadowVertex(position, lightPosition);

	normalVector = normalMatrix * normal;
    lightVector = (lightPosition - positionWorld.xyz);
    texCoord = textureCoords;
	gl_Position = positionTrans;
}
