#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texCoords;
//
out vec4 shadowMapPos;
out vec3 normalVector;
out vec3 lightVector;
out vec2 textureCoords;
//
uniform mat4 transformMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
//
uniform vec3 lightPosition = vec3(0, 0, 0);
//
uniform mat4 depthVP;
//

void main()
{
	vec4 positionWorld = transformMatrix * vec4(in_position, 1.0f);

	shadowMapPos = depthVP * positionWorld;

	normalVector = normalMatrix * in_normal;
	lightVector = (lightPosition - positionWorld.xyz);

	textureCoords = in_texCoords;

	gl_Position = projectionMatrix * viewMatrix * positionWorld;
}
