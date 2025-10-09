#version 410

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 position;
in vec3 normal;
//
out vec3 lightVector;
out vec3 normalVector;
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

	normalVector = normalMatrix * normal;
	lightVector = (lightPosition - positionWorld.xyz);

	gl_Position = projectionMatrix * viewMatrix * positionWorld;
}