#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

//
in vec3 position;
//
out vec3 fPos;
//
uniform mat4 transformMatrix;
uniform mat4 vpMatrix;
//
uniform vec3 camPosition = vec3(0.0f);
//
const float separationDis = 10.0f;

void main()
{
	vec4 positionWorld = transformMatrix * vec4(position, 1.0f);

	positionWorld.xyz += camPosition - separationDis;

    fPos = positionWorld.xyz;

	gl_Position = vpMatrix * positionWorld;
}