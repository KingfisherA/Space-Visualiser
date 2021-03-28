#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 position;
in vec3 normal;
in vec2 texCoord;
//
out vec3 lightVector;
out vec3 normalVector;
out vec2 textureCoord;
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
	textureCoord = texCoord;

	gl_Position = projectionMatrix * viewMatrix * positionWorld;
}