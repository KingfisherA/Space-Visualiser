#version 460

#include <Shaders/Universe/Planet/Ring Headers/Planet Rings.v.h.glsl>

in vec3 position;
in vec3 normal;
in vec2 textureCoords;
//
out vec3 lightVector;
out vec3 normalVector;
out vec3 toCameraVector;
out vec2 texCoords;
//
uniform mat4 transformMatrix;
uniform mat4 viewMatrix;
uniform mat4 inverseViewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
//
uniform vec3 lightPosition;

void main(void)
{
vec4 positionWorld = transformMatrix * vec4(position, 1.0f);

texCoords = textureCoords;

normalVector = normalMatrix * normal;
lightVector = (lightPosition - positionWorld.xyz);

RingShadowVertex(position, lightPosition);

  toCameraVector = (inverseViewMatrix * vec4(0.0,0.0,0.0,1.0)).xyz - positionWorld.xyz;

gl_Position =  projectionMatrix * viewMatrix * positionWorld;
}