#version 410

#pragma remove
#include "./Shaders/GLSL Language Specification.h.glsl"
#pragma endremove

layout(location = 0) in vec2 aVertex;

uniform mat4 uTransformMatrix;
uniform mat4 uViewProjMatrix;
//
uniform vec3 uLightPosition;
//

out vec3 vVertex;
out vec3 vLightVector;

void main()
{
    vec4 positionWorld = uTransformMatrix * vec4(aVertex, 0.0f, 1.0f);
    vVertex = positionWorld.xyz;

    vLightVector = uLightPosition;
    gl_Position = uViewProjMatrix * positionWorld;
}