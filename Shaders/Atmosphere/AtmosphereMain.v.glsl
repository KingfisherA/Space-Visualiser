#version 410

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

uniform mat4 uInverseViewMatrix;

uniform mat4 uTransformMatrix;
uniform mat4 uViewProjMatrix;
uniform mat3 uNormalMatrix;

uniform vec3 uSunPosition;
uniform vec3 uPlanetCentre;

layout(location = 0) in vec3 aVertex;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vVertex;
out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vToCamVector;
out vec2 vRingShadowParams; //(Ring shadow x-texture coordinate, Ring shadow fade)

const float bottomRad = 6360.0f;
const vec3 RING_PARAMS = vec3((1.0f / bottomRad), bottomRad + 300.0f, 1.0f);

void CalculateRingShadow(in vec3 lightPos, in vec3 vertexIn, in vec3 ringParamsIn, out vec2 ringShadowParamsOut)
{
  //vec3 lightPosA = ((-normalize(sunPosition) + vec3(0.0f, 0.2f, 0.0f)) * ATMOSPHERE.bottom_radius) + earth_center;

  vec3 lightPosA = lightPos - vertexIn.xyz;
  vec2 shadowProj = vertexIn.xz - lightPosA.xz * min(vertexIn.y / lightPosA.y, 0.0);
  float vRingShadowCoord = (length(shadowProj) - ringParamsIn.y) * ringParamsIn.x;
  float sina = abs(lightPos.y) * inversesqrt(dot(lightPos, lightPos));
  float vRingShadowFade = ringParamsIn.z * smoothstep(0.0003, 0.001, sina);

  ringShadowParamsOut = vec2(vRingShadowCoord, vRingShadowFade);
}

void main()
{
  vec4 positionWorld = uTransformMatrix * vec4(aVertex, 1.0f);

  vVertex = uNormalMatrix * aVertex; //Must not have any translation
  vNormal = uNormalMatrix * aNormal;
  vTexCoord = aTexCoord;
  vToCamVector = (uInverseViewMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz - positionWorld.xyz;

  vec3 transformedSunPos = ((-normalize(uSunPosition) + vec3(0.0f, 0.2f, 0.0f)) * bottomRad) + uPlanetCentre;
  vec3 transformedVertex = aVertex * bottomRad;
  CalculateRingShadow(transformedSunPos, transformedVertex, RING_PARAMS, vRingShadowParams);

  gl_Position = uViewProjMatrix * positionWorld;
}