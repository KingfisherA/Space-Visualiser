#version 440

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

uniform mat4 uViewProjMatrix;
uniform mat4 uTransformMatrix;
uniform mat3 uNormalMatrix;

layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec3 aNormal;
//layout(location = 2) in vec2 aTexCoord;

const float topRad = 6420.0f;

out vec3 vVertex;
out vec3 vNormal;

void main()
{
  vec4 positionWorld = uTransformMatrix * vec4(aVertex, 1.0f);

  vVertex = aVertex;
  vNormal = uNormalMatrix * aNormal;

  gl_Position = uViewProjMatrix * positionWorld;
}